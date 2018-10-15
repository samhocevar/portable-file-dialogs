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
#include <iostream>
#include <regex>
#include <cstdio>

#if _WIN32
#include <windows.h>
#else
#include <fcntl.h>  // for fcntl()
#include <unistd.h> // for read()
#endif

namespace pfd
{

enum class buttons
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

class executor
{
    friend class dialog;

    executor()
    {
    }

    executor(std::string const &command)
    {
        start(command);
    }

    void start(std::string const &command)
    {
        stop();
        exec_result.clear();

#if _WIN32
        STARTUPINFOW si;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::wstring wcommand = str2wstr(command);
        if (!CreateProcessW(nullptr, (LPWSTR)wcommand.c_str(), nullptr, nullptr,
                            FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &exec_pi))
        {
            exec_pi.hProcess = 0;
            return; /* TODO: GetLastError()? */
        }
#else
        exec_stream = popen(command.c_str(), "r");
        if (!exec_stream)
            return;

        exec_fd = fileno(exec_stream);
        fcntl(exec_fd, F_SETFL, O_NONBLOCK);
#endif
    }

    bool ready()
    {
        if (is_ready)
            return true;

#if _WIN32
        if (exec_pi.hProcess != 0)
        {
            WaitForInputIdle(exec_pi.hProcess, INFINITE);
            WaitForSingleObject(exec_pi.hProcess, INFINITE);
            CloseHandle(exec_pi.hThread);
            CloseHandle(exec_pi.hProcess);
        }
#else
        if (exec_stream)
        {
            char buf[BUFSIZ];
            ssize_t received = read(exec_fd, buf, BUFSIZ - 1);
            if (received == -1 && errno == EAGAIN)
                return false;
            if (received > 0)
            {
                buf[received] = '\0';
                exec_result += buf;
                return false;
            }
        }
#endif
        return is_ready = true;
    }

    std::string result(int *exit_code = nullptr)
    {
        int ret = stop();
        if (exit_code)
            *exit_code = ret;
        return exec_result;
    }

    int stop()
    {
        while (!ready())
            ; // TODO: spinlock?
#if _WIN32
        return exec_pi.hProcess == 0 ? -1 : 0;
#else
        return exec_stream ? pclose(exec_stream) : -1;
#endif
    }

    ~executor()
    {
        stop();
    }

private:
    bool is_ready = false;
    std::string exec_result;
#if _WIN32
    PROCESS_INFORMATION exec_pi;
#else
    FILE *exec_stream = nullptr;
    int exec_fd = -1;
#endif
};

class dialog
{
    friend class settings;
    friend class notify;
    friend class message;

protected:
    explicit dialog(bool resync = false)
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

#if _WIN32
    static std::wstring str2wstr(std::string const &str)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring ret(len, '\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)ret.data(), len);
        return ret;
    }

    static std::string wstr2str(std::wstring const &str)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string ret(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, (LPSTR)ret.data(), len, nullptr, nullptr);
        return ret;
    }
#endif

    std::string execute(std::string const &command, int *exit_code = nullptr)
    {
        if (flags(flag::is_verbose))
            std::cerr << "pfd: " << command << std::endl;

        return executor(command).result(exit_code);
    }

    std::string desktop_helper() const
    {
        return flags(flag::has_zenity) ? "zenity"
             : flags(flag::has_matedialog) ? "matedialog"
             : flags(flag::has_qarma) ? "qarma"
             : flags(flag::has_kdialog) ? "kdialog"
             : "echo";
    }

    std::string buttons_to_name(buttons buttons) const
    {
        switch (buttons)
        {
            case buttons::ok_cancel: return "okcancel";
            case buttons::yes_no: return "yesno";
            case buttons::yes_no_cancel: return "yesnocancel";
            /* case buttons::ok: */ default: return "ok";
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
    bool check_program(char const *program)
    {
#if _WIN32
        return false;
#else
        std::string command = "which ";
        command += program;
        command += " 2>/dev/null";

        int exit_code = -1;
        execute(command.c_str(), &exit_code);
        return exit_code == 0;
#endif
    }

    // Keep handle to executing command
    std::string exec_result;
};

class settings
{
public:
    static void verbose(bool value)
    {
        dialog().flags(dialog::flag::is_verbose) = value;
    }

    static void rescan()
    {
        dialog(true);
    }
};

class notify : protected dialog
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
        execute(command, &exit_code);
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

        execute(command, &exit_code);
#endif
    }

    int exit_code = -1;
};

class message : protected dialog
{
public:
    message(std::string const &title,
            std::string const &text,
            buttons buttons = buttons::ok_cancel,
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

        switch (buttons)
        {
            case buttons::ok_cancel: style |= MB_OKCANCEL; break;
            case buttons::yes_no: style |= MB_YESNO; break;
            case buttons::yes_no_cancel: style |= MB_YESNOCANCEL; break;
            /* case buttons::ok: */ default: style |= MB_OK; break;
        }

        auto wtitle = str2wstr(title);
        auto wmessage = str2wstr(text);
        auto ret = MessageBoxW(GetForegroundWindow(), wmessage.c_str(),
                               wtitle.c_str(), style);
#else
        auto command = desktop_helper();

        if (is_zenity())
        {
            switch (buttons)
            {
                case buttons::ok_cancel:
                    command += " --question --ok-label=OK --cancel-label=Cancel"; break;
                case buttons::yes_no:
                    command += " --question"; break;
                case buttons::yes_no_cancel:
                    command += " --list --column '' --hide-header 'Yes' 'No'"; break;
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
            if (buttons == buttons::ok)
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
                if (buttons == buttons::yes_no_cancel)
                    command += "cancel";
            }

            command += " " + shell_quote(text)
                     + " --title " + shell_quote(title);

            if (buttons == buttons::ok_cancel)
                command += " --yes-label OK --no-label Cancel";
        }

        result = execute(command, &exit_code);
#endif
    }

    std::string result;
    int exit_code = -1;
};

class file_dialog : protected dialog
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
        auto wtitle = str2wstr(title);

        OPENFILENAMEW ofn;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = GetForegroundWindow();
        if (!filter.empty())
        {
            auto wfilter = str2wstr(filter);
            ofn.lpstrFilter = wfilter.c_str();
            ofn.nFilterIndex = 1;
        }
        ofn.lpstrFile = (LPWSTR)wresult.data();
        ofn.nMaxFile = MAX_PATH;
        if (!default_path.empty())
        {
            auto wdefault_path = str2wstr(default_path);
            ofn.lpstrFileTitle = (LPWSTR)wdefault_path.data();
            ofn.nMaxFileTitle = MAX_PATH;
            ofn.lpstrInitialDir = wdefault_path.c_str();
        }
        ofn.lpstrTitle = wtitle.c_str();
        ofn.Flags = OFN_NOCHANGEDIR;
        if (in_type == type::open)
        {
            ofn.Flags |= OFN_PATHMUSTEXIST;
            exit_code = GetOpenFileNameW(&ofn);
        }
        else
        {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
            exit_code = GetSaveFileNameW(&ofn);
        }

        wresult.resize(wcslen(wresult.c_str()));
        result = wstr2str(wresult);
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

        result = execute(command, &exit_code);
#endif
    }

public:
    std::string result;
    int exit_code = -1;
};

class open_file : protected file_dialog
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

class save_file : protected file_dialog
{
public:
    save_file(std::string const &title,
              std::string const &default_path = "",
              std::string const &filter = "")
      : file_dialog(type::save, title, default_path, filter)
    {
    }
};

class select_folder : protected file_dialog
{
public:
    select_folder(std::string const &title,
                  std::string const &default_path = "")
      : file_dialog(type::folder, title, default_path)
    {
    }
};

} // namespace pfd

