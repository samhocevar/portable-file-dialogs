//
//  Portable File Dialogs
//
//  Copyright © 2018 Sam Hocevar <sam@hocevar.net>
//
//  This library is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <regex>
#include <thread>
#include <chrono>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <commdlg.h>
#include <future>
#else
#include <fcntl.h>  // for fcntl()
#include <unistd.h> // for read()
#endif

namespace pfd
{

// Forward declarations for our API
class settings;
class notify;
class message;

enum class button
{
    cancel = -1,
    ok,
    yes = ok,
    no,
};

enum class choice
{
    ok = 0,
    ok_cancel,
    yes_no,
    yes_no_cancel,
};

enum class icon
{
    info = 0,
    warning,
    error,
    question,
};

// Process wait timeout, in milliseconds
static int const default_wait_timeout = 20;

// Internal classes, not to be used by client applications
namespace internal
{

#if _WIN32
static inline std::wstring str2wstr(std::string const &str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring ret(len, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)ret.data(), len);
    return ret;
}

static inline std::string wstr2str(std::wstring const &str)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string ret(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, (LPSTR)ret.data(), len, nullptr, nullptr);
    return ret;
}
#endif

class executor
{
    friend class dialog;

public:
    // High level function to get the result of a command
    std::string result(int *exit_code = nullptr)
    {
        stop();
        if (exit_code)
            *exit_code = m_exit_code;
        return m_stdout;
    }

#if _WIN32
    void start(std::function<std::string(int *)> const &fun)
    {
        stop();
        m_future = std::async(fun, &m_exit_code);
        m_running = true;
    }
#endif

    void start(std::string const &command)
    {
        stop();
        m_stdout.clear();
        m_exit_code = -1;

#if _WIN32
        STARTUPINFOW si;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::wstring wcommand = str2wstr(command);
        if (!CreateProcessW(nullptr, (LPWSTR)wcommand.c_str(), nullptr, nullptr,
                            FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &m_pi))
            return; /* TODO: GetLastError()? */
        WaitForInputIdle(m_pi.hProcess, INFINITE);
#else
        m_stream = popen(command.c_str(), "r");
        if (!m_stream)
            return;
        m_fd = fileno(m_stream);
        fcntl(m_fd, F_SETFL, O_NONBLOCK);
#endif
        m_running = true;
    }

    ~executor()
    {
        stop();
    }

protected:
    bool ready(int timeout = default_wait_timeout)
    {
        if (!m_running)
            return true;

#if _WIN32
        if (m_future.valid())
        {
            auto status = m_future.wait_for(std::chrono::milliseconds(timeout));
            if (status != std::future_status::ready)
                return false;

            m_stdout = m_future.get();
        }
        else
        {
            if (WaitForSingleObject(m_pi.hProcess, timeout) == WAIT_TIMEOUT)
                return false;

            DWORD ret;
            GetExitCodeProcess(m_pi.hProcess, &ret);
            m_exit_code = (int)ret;
            CloseHandle(m_pi.hThread);
            CloseHandle(m_pi.hProcess);
        }
#else
        char buf[BUFSIZ];
        ssize_t received = read(m_fd, buf, BUFSIZ - 1);
        if (received == -1 && errno == EAGAIN)
        {
            // FIXME: this happens almost always at first iteration
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            return false;
        }
        if (received > 0)
        {
            m_stdout += std::string(buf, received);
            return false;
        }
        m_exit_code = pclose(m_stream);
#endif

        m_running = false;
        return true;
    }

    void stop()
    {
        while (!ready())
            ; // loop forever
    }

private:
    bool m_running = false;
    std::string m_stdout;
    int m_exit_code = -1;
#if _WIN32
    std::future<std::string> m_future;
    PROCESS_INFORMATION m_pi;
#else
    FILE *m_stream = nullptr;
    int m_fd = -1;
#endif
};

class dialog
{
    friend class pfd::settings;
    friend class pfd::notify;
    friend class pfd::message;

public:
    bool ready(int timeout = default_wait_timeout)
    {
        return m_async->ready(timeout);
    }

protected:
    explicit dialog(bool resync = false)
      : m_async(std::make_shared<executor>())
    {
        static bool analysed = false;
        if (resync || !analysed)
        {
#if !_WIN32
            flags(flag::has_zenity) = check_program("zenity");
            flags(flag::has_matedialog) = check_program("matedialog");
            flags(flag::has_qarma) = check_program("qarma");
            flags(flag::has_kdialog) = check_program("kdialog");
#endif
            analysed = true;
        }
    }

    enum class flag
    {
        is_verbose = 0,
        has_zenity,
        has_matedialog,
        has_qarma,
        has_kdialog,

        max_flag,
    };

    bool is_zenity() const
    {
        return flags(flag::has_zenity) ||
               flags(flag::has_matedialog) ||
               flags(flag::has_qarma);
    }

    bool is_kdialog() const
    {
        return flags(flag::has_kdialog);
    }

    // Static array of flags for internal state
    bool const &flags(flag in_flag) const
    {
        static bool flags[(size_t)flag::max_flag];
        return flags[(size_t)in_flag];
    }

    std::string execute(std::string const &command, int *exit_code = nullptr)
    {
        if (flags(flag::is_verbose))
            std::cerr << "pfd: " << command << std::endl;

        m_async->start(command);
        return m_async->result(exit_code);
    }

    std::string desktop_helper() const
    {
        return flags(flag::has_zenity) ? "zenity"
             : flags(flag::has_matedialog) ? "matedialog"
             : flags(flag::has_qarma) ? "qarma"
             : flags(flag::has_kdialog) ? "kdialog"
             : "echo";
    }

    std::string buttons_to_name(choice choice) const
    {
        switch (choice)
        {
            case choice::ok_cancel: return "okcancel";
            case choice::yes_no: return "yesno";
            case choice::yes_no_cancel: return "yesnocancel";
            /* case choice::ok: */ default: return "ok";
        }
    }

    std::string get_icon_name(icon icon) const
    {
        switch (icon)
        {
            case icon::warning: return "warning";
            case icon::error: return "error";
            case icon::question: return "question";
            // Zenity wants "information" but WinForms wants "info"
            /* case icon::info: */ default:
#if _WIN32
                return "info";
#else
                return "information";
#endif
        }
    }

    // Properly quote a string for Powershell: replace ' or " with '' or ""
    // FIXME: we should probably get rid of newlines!
    // FIXME: the \" sequence seems unsafe, too!
    std::string powershell_quote(std::string const &str) const
    {
        return "'" + std::regex_replace(str, std::regex("['\"]"), "$&$&") + "'";
    }

    // Properly quote a string for the shell: just replace ' with '\''
    std::string shell_quote(std::string const &str) const
    {
        return "'" + std::regex_replace(str, std::regex("'"), "'\\''") + "'";
    }

private:
    // Non-const getter for the static array of flags
    bool &flags(flag in_flag)
    {
        return const_cast<bool &>(static_cast<const dialog *>(this)->flags(in_flag));
    }

    // Check whether a program is present using “which”
    bool check_program(std::string const &program)
    {
#if _WIN32
        return false;
#else
        int exit_code = -1;
        execute("which " + program + " 2>/dev/null", &exit_code);
        return exit_code == 0;
#endif
    }

protected:
    // Keep handle to executing command
    std::shared_ptr<executor> m_async;
};

class file_dialog : public dialog
{
protected:
    enum type { open, save, folder, };

    file_dialog(type in_type,
                std::string const &title,
                std::string const &default_path = "",
                std::string const &filter = "",
                bool multiselect = false)
    {
#if _WIN32
        auto wresult = std::wstring(MAX_PATH, L'\0');
        auto wtitle = internal::str2wstr(title);

        OPENFILENAMEW ofn;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = GetForegroundWindow();
        if (!filter.empty())
        {
            auto wfilter = internal::str2wstr(filter);
            ofn.lpstrFilter = wfilter.c_str();
            ofn.nFilterIndex = 1;
        }
        ofn.lpstrFile = (LPWSTR)wresult.data();
        ofn.nMaxFile = MAX_PATH;
        if (!default_path.empty())
        {
            auto wdefault_path = internal::str2wstr(default_path);
            ofn.lpstrFileTitle = (LPWSTR)wdefault_path.data();
            ofn.nMaxFileTitle = MAX_PATH;
            ofn.lpstrInitialDir = wdefault_path.c_str();
        }
        ofn.lpstrTitle = wtitle.c_str();
        ofn.Flags = OFN_NOCHANGEDIR;
        if (in_type == type::open)
        {
            ofn.Flags |= OFN_PATHMUSTEXIST;
            int exit_code = GetOpenFileNameW(&ofn);
        }
        else
        {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
            int exit_code = GetSaveFileNameW(&ofn);
        }

        wresult.resize(wcslen(wresult.c_str()));
        /* m_stdout = */ internal::wstr2str(wresult);
#else
        auto command = desktop_helper();

        if (is_zenity())
        {
            command += " --file-selection --filename=" + shell_quote(default_path)
                     + " --title " + shell_quote(title)
                     + " --file-filter=" + shell_quote(filter)
                     + (multiselect ? " --multiple" : "");
            if (in_type == type::save)
                command += " --save";
        }

        m_async->start(command);
#endif
    }
};

} // namespace internal

class settings
{
public:
    static void verbose(bool value)
    {
        internal::dialog().flags(internal::dialog::flag::is_verbose) = value;
    }

    static void rescan()
    {
        internal::dialog(true);
    }
};

class notify : public internal::dialog
{
public:
    notify(std::string const &title,
           std::string const &message,
           icon icon = icon::info)
    {
        if (icon == icon::question) // Not supported by notifications
            icon = icon::info;

#if _WIN32
        int const delay = 5000;
        auto command = "powershell.exe -Command \""
                       "    Add-Type -AssemblyName System.Windows.Forms;"
                       "    $exe = (Get-Process -id " + std::to_string(GetCurrentProcessId()) + ").Path;"
                       "    $popup = New-Object System.Windows.Forms.NotifyIcon;"
                       "    $popup.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($exe);"
                       "    $popup.Visible = $true;"
                       "    $popup.ShowBalloonTip(" + std::to_string(delay) + ", "
                                                    + powershell_quote(title) + ", "
                                                    + powershell_quote(message) + ", "
                                                "'" + get_icon_name(icon) + "');"
                       "    Start-Sleep -Milliseconds " + std::to_string(delay) + ";"
                       "    $popup.Dispose();" // Ensure the icon is cleaned up, but not too soon.
                       "\"";
#else
        auto command = desktop_helper();

        if (is_zenity())
        {
            command += " --notification"
                       " --window-icon " + get_icon_name(icon) +
                       " --text " + shell_quote(title + "\n" + message);
        }
        else if (is_kdialog())
        {
            command += " --icon " + get_icon_name(icon) +
                       " --title " + shell_quote(title) +
                       " --passivepopup " + shell_quote(message) +
                       " 5";
        }
#endif
        m_async->start(command);
    }
};

class message : public internal::dialog
{
public:
    message(std::string const &title,
            std::string const &text,
            choice choice = choice::ok_cancel,
            icon icon = icon::info)
    {
#if _WIN32
        UINT style = MB_TOPMOST;
        switch (icon)
        {
            case icon::warning: style |= MB_ICONWARNING; break;
            case icon::error: style |= MB_ICONERROR; break;
            case icon::question: style |= MB_ICONQUESTION; break;
            /* case icon::info: */ default: style |= MB_ICONINFORMATION; break;
        }

        switch (choice)
        {
            case choice::ok_cancel: style |= MB_OKCANCEL; break;
            case choice::yes_no: style |= MB_YESNO; break;
            case choice::yes_no_cancel: style |= MB_YESNOCANCEL; break;
            /* case choice::ok: */ default: style |= MB_OK; break;
        }

        m_mappings[IDCANCEL] = button::cancel;
        m_mappings[IDOK] = button::ok;
        m_mappings[IDYES] = button::yes;
        m_mappings[IDNO] = button::no;

        m_async->start([text, title, style](int *exit_code) -> std::string
        {
            auto wtext = internal::str2wstr(text);
            auto wtitle = internal::str2wstr(title);
            *exit_code = MessageBoxW(GetForegroundWindow(), wtext.c_str(), wtitle.c_str(), style);
            return "";
        });
#else
        auto command = desktop_helper();

        if (is_zenity())
        {
            switch (choice)
            {
                case choice::ok_cancel:
                    command += " --question --ok-label=OK --cancel-label=Cancel"; break;
                case choice::yes_no:
                    // Do not use standard --question because it causes “No” to return -1,
                    // which is inconsistent with the “Yes/No/Cancel” mode below.
                    command += " --question --switch --extra-button No --extra-button Yes"; break;
                case choice::yes_no_cancel:
                    command += " --question --switch --extra-button No --extra-button Yes --extra-button Cancel"; break;
                default:
                    switch (icon)
                    {
                        case icon::error: command += " --error"; break;
                        case icon::warning: command += " --warning"; break;
                        default: command += " --info"; break;
                    }
            }

            command += " --title " + shell_quote(title)
                     + " --width 300 --height 0" // sensible defaults
                     + " --text " + shell_quote(text)
                     + " --icon-name=dialog-" + get_icon_name(icon);
        }
        else if (is_kdialog())
        {
            if (choice == choice::ok)
            {
                switch (icon)
                {
                    case icon::error: command += " --error"; break;
                    case icon::warning: command += " --sorry"; break;
                    default: command += " --msgbox"; break;
                }
            }
            else
            {
                command += " --";
                if (icon == icon::warning || icon == icon::error)
                    command += "warning";
                command += "yesno";
                if (choice == choice::yes_no_cancel)
                {
                    m_mappings[256] = button::no;
                    command += "cancel";
                }
            }

            command += " " + shell_quote(text)
                     + " --title " + shell_quote(title);

            // Must be after the above part
            if (choice == choice::ok_cancel)
                command += " --yes-label OK --no-label Cancel";
        }

        m_async->start(command);
#endif
    }

    button result()
    {
        int exit_code;
        auto ret = m_async->result(&exit_code);
        if (exit_code < 0 /* this means cancel */ || ret == "Cancel\n")
            return button::cancel;
        if (ret == "Yes\n" || ret == "OK\n")
            return button::ok;
        if (ret == "No\n")
            return button::no;
        if (m_mappings.count(exit_code) != 0)
            return m_mappings[exit_code];
        return exit_code == 0 ? button::ok : button::cancel;
    }

private:
    // Some extra logic to map the exit code to button number
    std::map<int, button> m_mappings;
};

class open_file : public internal::file_dialog
{
public:
    open_file(std::string const &title,
              std::string const &default_path = "",
              std::string const &filter = "",
              bool multiselect = false)
      : file_dialog(type::open, title, default_path, filter, multiselect)
    {
    }
};

class save_file : public internal::file_dialog
{
public:
    save_file(std::string const &title,
              std::string const &default_path = "",
              std::string const &filter = "")
      : file_dialog(type::save, title, default_path, filter)
    {
    }
};

class select_folder : public internal::file_dialog
{
public:
    select_folder(std::string const &title,
                  std::string const &default_path = "")
      : file_dialog(type::folder, title, default_path)
    {
    }
};

} // namespace pfd

