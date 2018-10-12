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
#include <regex>
#include <cstdio>

#if _WIN32
#include <windows.h>
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

class dialog
{
    friend class notify;
    friend class message;

protected:
#if !_WIN32
    dialog(bool resync = false)
    {
        static bool analysed = false;
        if (resync || !analysed)
        {
            flags(flag::has_zenity) = check_program("zenity");
            flags(flag::has_matedialog) = check_program("matedialog");
            flags(flag::has_shellementary) = check_program("shellementary");
            flags(flag::has_qarma) = check_program("qarma");
            flags(flag::has_kdialog) = check_program("kdialog");
        }
    }

    enum class flag
    {
        has_zenity = 0,
        has_matedialog,
        has_shellementary,
        has_qarma,
        has_kdialog,

        max_flag,
    };

    bool is_zenity() const
    {
        return flags(flag::has_zenity) ||
               flags(flag::has_matedialog) ||
               flags(flag::has_shellementary) ||
               flags(flag::has_qarma);
    }

    bool is_kdialog() const
    {
        return flags(flag::has_kdialog);
    }

    // Static array of flags for internal state
    bool const &flags(flag flag) const
    {
        static bool flags[(size_t)flag::max_flag];
        return flags[(size_t)flag];
    }
#endif

#if _WIN32
    void execute(std::string const &command, int *exit_code = nullptr) const
    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::wstring wcommand = str2wstr(command);
        if (!CreateProcessW(nullptr, (LPWSTR)wcommand.c_str(), nullptr, nullptr,
                            FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi))
        {
            if (exit_code)
                *exit_code = -1;
            return; /* GetLastError(); */
        }

        WaitForInputIdle(pi.hProcess, INFINITE);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (exit_code)
            *exit_code = 0;
    }

    static std::wstring str2wstr(std::string const &str)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring ret(len, '\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)ret.data(), len);
        return ret;
    }
#else
    std::string execute(std::string const &command, int *exit_code = nullptr) const
    {
        auto stream = popen(command.c_str(), "r");
        if (!stream)
        {
            if (exit_code)
                *exit_code = -1;
            return "";
        }

        std::string result;
        while (!feof(stream))
        {
            char buf[BUFSIZ];
            fgets(buf, BUFSIZ, stream);
            result += buf;
        }

        if (exit_code)
            *exit_code = pclose(stream);
        else
            pclose(stream);

        return result;
    }

    std::string helper_command() const
    {
        return flags(flag::has_zenity) ? "zenity"
             : flags(flag::has_matedialog) ? "matedialog"
             : flags(flag::has_shellementary) ? "shellementary"
             : flags(flag::has_qarma) ? "qarma"
             : flags(flag::has_kdialog) ? "kdialog"
             : "echo";
    }

    std::string buttons_to_name(buttons buttons) const
    {
        switch (buttons)
        {
            case buttons::ok: default: return "ok";
            case buttons::ok_cancel: return "okcancel";
            case buttons::yes_no: return "yesno";
            case buttons::yes_no_cancel: return "yesnocancel";
        }
    }
#endif

    std::string get_icon_name(icon icon) const
    {
        switch (icon)
        {
            // Zenity wants "information" but Powershell wants "info"
#if _WIN32
            case icon::info: default: return "info";
#else
            case icon::info: default: return "information";
#endif
            case icon::warning: return "warning";
            case icon::error: return "error";
            case icon::question: return "question";
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
#if _WIN32

#else
    // Non-const getter for the static array of flags
    bool &flags(flag flag)
    {
        return const_cast<bool &>(static_cast<const dialog *>(this)->flags(flag));
    }

    // Check whether a program is present using “which”
    bool check_program(char const *program)
    {
        std::string command = "which ";
        command += program;
        command += " 2>/dev/null";

        int exit_code = -1;
        execute(command.c_str(), &exit_code);
        return exit_code == 0;
    }
#endif
};

class notify : dialog
{
public:
    notify(std::string const &title,
           std::string const &message,
           icon icon = icon::info)
    {
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
        if (icon == icon::question) // Not supported by notifications
            icon = icon::info;

        auto command = helper_command();

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

class message : dialog
{
public:
    message(std::string const &title,
           std::string const &message,
           buttons buttons = buttons::ok_cancel,
           icon icon = icon::info)
    {
#if _WIN32
        UINT style = MB_TOPMOST;
        switch (icon)
        {
            case icon::info: default: style |= MB_ICONINFORMATION; break;
            case icon::warning: style |= MB_ICONWARNING; break;
            case icon::error: style |= MB_ICONERROR; break;
            case icon::question: style |= MB_ICONQUESTION; break;
        }

        switch (buttons)
        {
            case buttons::ok: default: style |= MB_OK; break;
            case buttons::ok_cancel: style |= MB_OKCANCEL; break;
            case buttons::yes_no: style |= MB_YESNO; break;
            case buttons::yes_no_cancel: style |= MB_YESNOCANCEL; break;
        }

        auto wtitle = str2wstr(title);
        auto wmessage = str2wstr(message);
        auto ret = MessageBoxW(GetForegroundWindow(), wmessage.c_str(),
                               wtitle.c_str(), style);
#else
        auto command = helper_command();

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
                     + " --text " + shell_quote(message)
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

            command += " " + shell_quote(message)
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

} // namespace pfd

