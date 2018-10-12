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
    dialog(bool resync = false)
    {
        static bool analysed = false;
        if (resync || !analysed)
        {
            flags(flag::has_zenity) = check_program("zenity");
            flags(flag::has_matedialog) = check_program("matedialog");
            flags(flag::has_shellementary) = check_program("shellementary");
            flags(flag::has_qarma) = check_program("qarma");
        }
    }

    enum class flag
    {
        has_zenity = 0,
        has_matedialog,
        has_shellementary,
        has_qarma,

        max_flag,
    };

    // Static array of flags for internal state
    bool const &flags(flag flag) const
    {
        static bool flags[(size_t)flag::max_flag];
        return flags[(size_t)flag];
    }

    // Non-const getter for the static array of flags
    bool &flags(flag flag)
    {
        return const_cast<bool &>(static_cast<const dialog *>(this)->flags(flag));
    }

    std::string execute(std::string const &command, int *exit_code = nullptr)
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

    std::string get_helper_name() const
    {
        return flags(flag::has_zenity) ? "zenity"
             : flags(flag::has_matedialog) ? "matedialog"
             : flags(flag::has_shellementary) ? "shellementary"
             : flags(flag::has_qarma) ? "qarma"
             : "echo";
    }

    std::string get_buttons_name(buttons buttons) const
    {
        switch (buttons)
        {
            case buttons::ok: default: return "ok";
            case buttons::ok_cancel: return "okcancel";
            case buttons::yes_no: return "yesno";
            case buttons::yes_no_cancel: return "yesnocancel";
        }
    }

    std::string get_icon_name(icon icon) const
    {
        switch (icon)
        {
            case icon::info: default: return "information";
            case icon::warning: return "warning";
            case icon::error: return "error";
            case icon::question: return "question";
        }
    }

    // Properly quote a string for the shell
    std::string shell_quote(std::string const &s) const
    {
        return "'" + std::regex_replace(s, std::regex("'"), "'\\''") + "'";
    }

private:
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
};

class notify : dialog
{
public:
    notify(std::string const &title,
           std::string const &message,
           icon icon = icon::info)
    {
        if (icon == icon::question)
            icon = icon::info;

        auto command = get_helper_name()
                     + " --notification"
                     + " --window-icon " + get_icon_name(icon)
                     + " --text " + shell_quote(title + "\n" + message);
        result = execute(command, &exit_code);
    }

    std::string result;
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
        auto command = get_helper_name();
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

        result = execute(command, &exit_code);
    }

    std::string result;
    int exit_code = -1;
};

} // namespace pfd

