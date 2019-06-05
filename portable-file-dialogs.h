//
//  Portable File Dialogs
//
//  Copyright © 2018—2019 Sam Hocevar <sam@hocevar.net>
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
#include <shlobj.h>
#include <future>
#else
#include <cstdlib>  // for std::getenv()
#include <fcntl.h>  // for fcntl()
#include <unistd.h> // for read()
#endif

namespace pfd
{

enum class button
{
    cancel = -1,
    ok,
    yes,
    no,
    abort,
    retry,
    ignore,
};

enum class choice
{
    ok = 0,
    ok_cancel,
    yes_no,
    yes_no_cancel,
    retry_cancel,
    abort_retry_ignore,
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

// The settings class, only exposing to the user a way to set verbose mode
// and to force a rescan of installed desktop helpers (zenity, kdialog…).
class settings
{
public:
    static void verbose(bool value)
    {
        settings().flags(flag::is_verbose) = value;
    }

    static void rescan()
    {
        settings(true);
    }

protected:
    enum class flag
    {
        is_scanned = 0,
        is_verbose,

        has_zenity,
        has_matedialog,
        has_qarma,
        has_kdialog,

        max_flag,
    };

    explicit settings(bool resync = false)
    {
        flags(flag::is_scanned) &= !resync;
    }

    inline bool is_osascript() const
    {
#if __APPLE__
        return true;
#else
        return false;
#endif
    }

    inline bool is_zenity() const
    {
        return flags(flag::has_zenity) ||
               flags(flag::has_matedialog) ||
               flags(flag::has_qarma);
    }

    inline bool is_kdialog() const
    {
        return flags(flag::has_kdialog);
    }

    // Static array of flags for internal state
    bool const &flags(flag in_flag) const
    {
        static bool flags[(size_t)flag::max_flag];
        return flags[(size_t)in_flag];
    }

    // Non-const getter for the static array of flags
    bool &flags(flag in_flag)
    {
        return const_cast<bool &>(static_cast<const settings *>(this)->flags(in_flag));
    }
};

// Forward declarations for our API
class notify;
class message;

// Internal classes, not to be used by client applications
namespace internal
{

#if _WIN32
static inline std::wstring str2wstr(std::string const &str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring ret(len, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), (LPWSTR)ret.data(), (int)ret.size());
    return ret;
}

static inline std::string wstr2str(std::wstring const &str)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0, nullptr, nullptr);
    std::string ret(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), (LPSTR)ret.data(), (int)ret.size(), nullptr, nullptr);
    return ret;
}
#endif

// This is necessary until C++20 which will have std::string::ends_with() etc.
static inline bool ends_with(std::string const &str, std::string const &suffix)
{
    return suffix.size() <= str.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static inline bool starts_with(std::string const &str, std::string const &prefix)
{
    return prefix.size() <= str.size() &&
        str.compare(0, prefix.size(), prefix) == 0;
}

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
#elif __EMSCRIPTEN__ || __NX__
        // FIXME: do something
#else
        m_stream = popen((command + " 2>/dev/null").c_str(), "r");
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
#elif __EMSCRIPTEN__ || __NX__
        // FIXME: do something
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
        // Loop until the user closes the dialog
        while (!ready())
        {
#if _WIN32
            // On Windows, we need to run the message pump. If the async
            // thread uses a Windows API dialog, it may be attached to the
            // main thread and waiting for messages that only we can dispatch.
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
#endif
        }
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

class dialog : protected settings
{
    friend class pfd::notify;
    friend class pfd::message;

public:
    bool ready(int timeout = default_wait_timeout)
    {
        return m_async->ready(timeout);
    }

protected:
    explicit dialog()
      : m_async(std::make_shared<executor>())
    {
        if (!flags(flag::is_scanned))
        {
#if !__APPLE && !_WIN32
            flags(flag::has_zenity) = check_program("zenity");
            flags(flag::has_matedialog) = check_program("matedialog");
            flags(flag::has_qarma) = check_program("qarma");
            flags(flag::has_kdialog) = check_program("kdialog");

            // If multiple helpers are available, try to default to the best one
            if (flags(flag::has_zenity) && flags(flag::has_kdialog))
            {
                auto desktop_name = std::getenv("XDG_SESSION_DESKTOP");
                if (desktop_name && desktop_name == std::string("gnome"))
                    flags(flag::has_kdialog) = false;
                else if (desktop_name && desktop_name == std::string("KDE"))
                    flags(flag::has_zenity) = false;
            }
#endif
            flags(flag::is_scanned) = true;
        }
    }

    std::string desktop_helper() const
    {
#if __APPLE__
        return "osascript";
#else
        return flags(flag::has_zenity) ? "zenity"
             : flags(flag::has_matedialog) ? "matedialog"
             : flags(flag::has_qarma) ? "qarma"
             : flags(flag::has_kdialog) ? "kdialog"
             : "echo";
#endif
    }

    std::string buttons_to_name(choice choice) const
    {
        switch (choice)
        {
            case choice::ok_cancel: return "okcancel";
            case choice::yes_no: return "yesno";
            case choice::yes_no_cancel: return "yesnocancel";
            case choice::retry_cancel: return "retrycancel";
            case choice::abort_retry_ignore: return "abortretryignore";
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

    // Properly quote a string for osascript: replace ' with '\'' and \ or " with \\ or \"
    std::string osascript_quote(std::string const &str) const
    {
        return "\"" + std::regex_replace(std::regex_replace(str,
                          std::regex("[\\\\\"]"), "\\$&"), std::regex("'"), "'\\''") + "\"";
    }

    // Properly quote a string for the shell: just replace ' with '\''
    std::string shell_quote(std::string const &str) const
    {
        return "'" + std::regex_replace(str, std::regex("'"), "'\\''") + "'";
    }

    // Check whether a program is present using “which”
    bool check_program(std::string const &program)
    {
#if _WIN32
        (void)program;
        return false;
#else
        int exit_code = -1;
        m_async->start("which " + program + " 2>/dev/null");
        m_async->result(&exit_code);
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
    enum type
    {
        open,
        save,
        folder,
    };

    file_dialog(type in_type,
                std::string const &title,
                std::string const &default_path = "",
                std::vector<std::string> filters = {},
                bool allow_multiselect = false,
                bool confirm_overwrite = true)
    {
#if _WIN32
        std::string filter_list;
        std::regex whitespace("  *");
        for (size_t i = 0; i + 1 < filters.size(); i += 2)
        {
            filter_list += filters[i] + '\0';
            filter_list += std::regex_replace(filters[i + 1], whitespace, ";") + '\0';
        }
        filter_list += '\0';

        m_async->start([this, in_type, title, default_path, filter_list,
                        allow_multiselect, confirm_overwrite](int *exit_code) -> std::string
        {
            (void)exit_code;
            auto wtitle = internal::str2wstr(title);
            auto wfilter_list = internal::str2wstr(filter_list);
            auto wdefault_path = internal::str2wstr(default_path);

            // Folder selection uses a different method
            if (in_type == type::folder)
            {
                BROWSEINFOW bi;
                memset(&bi, 0, sizeof(bi));
                auto status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

                auto callback = [&](HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) -> INT
                {
                    switch (uMsg)
                    {
                        case BFFM_INITIALIZED:
                            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)wdefault_path.c_str());
                            break;
                        case BFFM_SELCHANGED:
                            // FIXME: this doesn’t seem to work wuth BIF_NEWDIALOGSTYLE
                            SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)wtitle.c_str());
                            break;
                    }
                    return 0;
                };

                bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) -> INT
                {
                    return (*(decltype(&callback))pData)(hwnd, uMsg, lp, 0);
                };
                bi.lParam = (LPARAM)&callback;

                if (status == S_OK)
                    bi.ulFlags |= BIF_NEWDIALOGSTYLE;
                bi.ulFlags |= BIF_EDITBOX;
                bi.ulFlags |= BIF_STATUSTEXT;
                auto *list = SHBrowseForFolderW(&bi);
                std::string ret;
                if (list)
                {
                    auto buffer = new wchar_t[MAX_PATH];
                    SHGetPathFromIDListW(list, buffer);
                    CoTaskMemFree(list);
                    ret = internal::wstr2str(buffer);
                    delete[] buffer;
                }
                if (status == S_OK)
                    CoUninitialize();
                return ret;
            }

            OPENFILENAMEW ofn;
            memset(&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(OPENFILENAMEW);
            ofn.hwndOwner = GetForegroundWindow();

            ofn.lpstrFilter = wfilter_list.c_str();

            auto woutput = std::wstring(MAX_PATH * 256, L'\0');
            ofn.lpstrFile = (LPWSTR)woutput.data();
            ofn.nMaxFile = (DWORD)woutput.size();
            if (!wdefault_path.empty())
            {
                // Initial directory
                ofn.lpstrInitialDir = wdefault_path.c_str();
                // Initial file selection
                ofn.lpstrFileTitle = (LPWSTR)wdefault_path.data();
                ofn.nMaxFileTitle = (DWORD)wdefault_path.size();
            }
            ofn.lpstrTitle = wtitle.c_str();
            ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER;

            if (in_type == type::save)
            {
                if (confirm_overwrite)
                    ofn.Flags |= OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn) == 0)
                    return "";
                return internal::wstr2str(woutput.c_str());
            }

            if (allow_multiselect)
                ofn.Flags |= OFN_ALLOWMULTISELECT;
            ofn.Flags |= OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn) == 0)
                return "";

            std::string prefix;
            for (wchar_t const *p = woutput.c_str(); *p; )
            {
                auto filename = internal::wstr2str(p);
                p += filename.size();
                // In multiselect mode, we advance p one step more and
                // check for another filename. If there is one and the
                // prefix is empty, it means we just read the prefix.
                if (allow_multiselect && *++p && prefix.empty())
                {
                    prefix = filename + "/";
                    continue;
                }

                m_vector_result.push_back(prefix + filename);
            }

            return "";
        });
#else
        auto command = desktop_helper();

        if (is_osascript())
        {
            command += " -e 'set ret to choose";
            switch (in_type)
            {
                case type::save:
                    command += " file name";
                    break;
                case type::open: default:
                    command += " file";
                    if (allow_multiselect)
                        command += " with multiple selections allowed";
                    break;
                case type::folder:
                    command += " folder";
                    break;
            }

            if (default_path.size())
                command += " default location " + osascript_quote(default_path);
            command += " with prompt " + osascript_quote(title);

            if (in_type == type::open)
            {
                // Concatenate all user-provided filter patterns
                std::string patterns;
                for (size_t i = 0; i < filters.size() / 2; ++i)
                    patterns += " " + filters[2 * i + 1];

                // Split the pattern list to check whether "*" is in there; if it
                // is, we have to disable filters because there is no mechanism in
                // OS X for the user to override the filter.
                std::regex sep("\\s+");
                std::string filter_list;
                bool has_filter = true;
                std::sregex_token_iterator iter(patterns.begin(), patterns.end(), sep, -1);
                std::sregex_token_iterator end;
                for ( ; iter != end; ++iter)
                {
                    auto pat = iter->str();
                    if (pat == "*" || pat == "*.*")
                        has_filter = false;
                    else if (internal::starts_with(pat, "*."))
                        filter_list += (filter_list.size() == 0 ? "" : ",") +
                                       osascript_quote(pat.substr(2, pat.size() - 2));
                }
                if (has_filter && filter_list.size() > 0)
                    command += " of type {" + filter_list + "}";
            }

            if (in_type == type::open && allow_multiselect)
            {
                command += "\nset s to \"\"";
                command += "\nrepeat with i in ret";
                command += "\n  set s to s & (POSIX path of i) & \"\\n\"";
                command += "\nend repeat";
                command += "\ncopy s to stdout'";
            }
            else
            {
                command += "\nPOSIX path of ret'";
            }
        }
        else if (is_zenity())
        {
            command += " --file-selection --filename=" + shell_quote(default_path)
                     + " --title " + shell_quote(title)
                     + " --separator='\n'";

            for (size_t i = 0; i < filters.size() / 2; ++i)
                command += " --file-filter " + shell_quote(filters[2 * i] + "|" + filters[2 * i + 1]);

            if (in_type == type::save)
                command += " --save";
            if (in_type == type::folder)
                command += " --directory";
            if (confirm_overwrite)
                command += " --confirm-overwrite";
            if (allow_multiselect)
                command += " --multiple";
        }
        else if (is_kdialog())
        {
            switch (in_type)
            {
                case type::save: command += " --getsavefilename"; break;
                case type::open: command += " --getopenfilename"; break;
                case type::folder: command += " --getexistingdirectory"; break;
            }
            command += " " + shell_quote(default_path);

            std::string filter;
            for (size_t i = 0; i < filters.size() / 2; ++i)
                filter += (i == 0 ? "" : " | ") + filters[2 * i] + "(" + filters[2 * i + 1] + ")";
            command += " " + shell_quote(filter);

            command += " --title " + shell_quote(title);
        }

        if (flags(flag::is_verbose))
            std::cerr << "pfd: " << command << std::endl;

        m_async->start(command);
#endif
    }

protected:
    std::string string_result()
    {
#if _WIN32
        return m_async->result();
#else
        // Strip the newline character
        auto ret = m_async->result();
        return ret.back() == '\n' ? ret.substr(0, ret.size() - 1) : ret;
#endif
    }

    std::vector<std::string> vector_result()
    {
#if _WIN32
        m_async->result();
        return m_vector_result;
#else
        std::vector<std::string> ret;
        auto result = m_async->result();
        for (;;)
        {
            // Split result along newline characters
            auto i = result.find('\n');
            if (i == 0 || i == std::string::npos)
                break;
            ret.push_back(result.substr(0, i));
            result = result.substr(i + 1, result.size());
        }
        return ret;
#endif
    }

#if _WIN32
    std::vector<std::string> m_vector_result;
#endif
};

} // namespace internal

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
        auto script = "Add-Type -AssemblyName System.Windows.Forms;"
                      "$exe = (Get-Process -id " + std::to_string(GetCurrentProcessId()) + ").Path;"
                      "$popup = New-Object System.Windows.Forms.NotifyIcon;"
                      "$popup.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($exe);"
                      "$popup.Visible = $true;"
                      "$popup.ShowBalloonTip(" + std::to_string(delay) + ", "
                                               + powershell_quote(title) + ", "
                                               + powershell_quote(message) + ", "
                                           "'" + get_icon_name(icon) + "');"
                      "Start-Sleep -Milliseconds " + std::to_string(delay) + ";"
                      "$popup.Dispose();"; // Ensure the icon is cleaned up, but not too soon.
        // Double fork to ensure the powershell script runs in the background
        auto command = "powershell.exe -Command \""
                       "    start-process powershell"
                       "        -ArgumentList " + powershell_quote(script) +
                       "        -WindowStyle hidden"
                       "\"";
#else
        auto command = desktop_helper();

        if (is_osascript())
        {
            command += " -e 'display notification " + osascript_quote(message) +
                       "     with title " + osascript_quote(title) + "'";
        }
        else if (is_zenity())
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

        if (flags(flag::is_verbose))
            std::cerr << "pfd: " << command << std::endl;

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
            case choice::retry_cancel: style |= MB_RETRYCANCEL; break;
            case choice::abort_retry_ignore: style |= MB_ABORTRETRYIGNORE; break;
            /* case choice::ok: */ default: style |= MB_OK; break;
        }

        m_mappings[IDCANCEL] = button::cancel;
        m_mappings[IDOK] = button::ok;
        m_mappings[IDYES] = button::yes;
        m_mappings[IDNO] = button::no;
        m_mappings[IDABORT] = button::abort;
        m_mappings[IDRETRY] = button::retry;
        m_mappings[IDIGNORE] = button::ignore;

        m_async->start([text, title, style](int *exit_code) -> std::string
        {
            auto wtext = internal::str2wstr(text);
            auto wtitle = internal::str2wstr(title);
            *exit_code = MessageBoxW(GetForegroundWindow(), wtext.c_str(), wtitle.c_str(), style);
            return "";
        });
#else
        auto command = desktop_helper();

        if (is_osascript())
        {
            command += " -e 'display dialog " + osascript_quote(text) +
                       "     with title " + osascript_quote(title);
            switch (choice)
            {
                case choice::ok_cancel:
                    command += "buttons {\"OK\", \"Cancel\"} "
                               "default button \"OK\" "
                               "cancel button \"Cancel\"";
                    m_mappings[256] = button::cancel;
                    break;
                case choice::yes_no:
                    command += "buttons {\"Yes\", \"No\"} "
                               "default button \"Yes\" "
                               "cancel button \"No\"";
                    m_mappings[256] = button::no;
                    break;
                case choice::yes_no_cancel:
                    command += "buttons {\"Yes\", \"No\", \"Cancel\"} "
                               "default button \"Yes\" "
                               "cancel button \"Cancel\"";
                    m_mappings[256] = button::cancel;
                    break;
                case choice::retry_cancel:
                    command += "buttons {\"Retry\", \"Cancel\"} "
                        "default button \"Retry\" "
                        "cancel button \"Cancel\"";
                    m_mappings[256] = button::cancel;
                    break;
                case choice::abort_retry_ignore:
                    command += "buttons {\"Abort\", \"Retry\", \"Ignore\"} "
                        "default button \"Retry\" "
                        "cancel button \"Retry\"";
                    m_mappings[256] = button::cancel;
                    break;
                case choice::ok: default:
                    command += "buttons {\"OK\"} "
                               "default button \"OK\" "
                               "cancel button \"OK\"";
                    m_mappings[256] = button::ok;
                    break;
            }
            command += " with icon ";
            switch (icon)
            {
                #define PFD_OSX_ICON(n) "alias ((path to library folder from system domain) as text " \
                    "& \"CoreServices:CoreTypes.bundle:Contents:Resources:" n ".icns\")"
                case icon::info: default: command += PFD_OSX_ICON("ToolBarInfo"); break;
                case icon::warning: command += "caution"; break;
                case icon::error: command += "stop"; break;
                case icon::question: command += PFD_OSX_ICON("GenericQuestionMarkIcon"); break;
                #undef PFD_OSX_ICON
            }
            command += "'";
        }
        else if (is_zenity())
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
                case choice::retry_cancel:
                    command += " --question --switch --extra-button Retry --extra-button Cancel"; break;
                case choice::abort_retry_ignore:
                    command += " --question --switch --extra-button Abort --extra-button Retry --extra-button Ignore"; break;
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

        if (flags(flag::is_verbose))
            std::cerr << "pfd: " << command << std::endl;

        m_async->start(command);
#endif
    }

    button result()
    {
        int exit_code;
        auto ret = m_async->result(&exit_code);
        // osascript will say "button returned:Cancel\n"
        // and others will just say "Cancel\n"
        if (exit_code < 0 || // this means cancel
            internal::ends_with(ret, "Cancel\n"))
            return button::cancel;
        if (internal::ends_with(ret, "OK\n"))
            return button::ok;
        if (internal::ends_with(ret, "Yes\n"))
            return button::yes;
        if (internal::ends_with(ret, "No\n"))
            return button::no;
        if (internal::ends_with(ret, "Abort\n"))
            return button::abort;
        if (internal::ends_with(ret, "Retry\n"))
            return button::retry;
        if (internal::ends_with(ret, "Ignore\n"))
            return button::ignore;
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
              std::vector<std::string> filters = { "All Files", "*" },
              bool allow_multiselect = false)
      : file_dialog(type::open, title, default_path,
                    filters, allow_multiselect, false)
    {
    }

    std::vector<std::string> result()
    {
        return vector_result();
    }
};

class save_file : public internal::file_dialog
{
public:
    save_file(std::string const &title,
              std::string const &default_path = "",
              std::vector<std::string> filters = { "All Files", "*" },
              bool confirm_overwrite = true)
      : file_dialog(type::save, title, default_path,
                    filters, false, confirm_overwrite)
    {
    }

    std::string result()
    {
        return string_result();
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

    std::string result()
    {
        return string_result();
    }
};

} // namespace pfd

