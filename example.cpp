//
//  Portable File Dialogs
//
//  Copyright © 2018 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#include "portable-file-dialogs.h"

#include <iostream>

int main()
{
    // Set verbosity to true
    pfd::settings::verbose(true);

    // Notification
    pfd::notify("Important Notification",
                "This is a message, pay attention to it!",
                pfd::icon::info);

    // Message box with nice message
    auto m = pfd::message("Personal Message",
                          "You are an amazing person, don’t let anyone make you think otherwise.",
                          pfd::choice::yes_no_cancel,
                          pfd::icon::warning);

    // Optional: do something while waiting for user action
    while (!m.ready(1000))
        std::cout << "Waited 1 second for user input...\n";

    // Do something according to the selected button
    switch (m.result())
    {
        case pfd::button::yes: std::cout << "User agreed.\n"; break;
        case pfd::button::no: std::cout << "User disagreed.\n"; break;
        case pfd::button::cancel: std::cout << "User freaked out.\n"; break;
    }

    // File open
    auto f = pfd::open_file("Choose files to read", "/tmp/",
                            { "Text Files (.txt .text)", "*.txt *.text", "All Files", "*" },
                            true);
    std::cout << "Selected files:";
    for (auto const &name : f.result())
        std::cout << " " + name;
    std::cout << "\n";
}

