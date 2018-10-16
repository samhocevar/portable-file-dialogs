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
                "This is a notification, pay attention to it!",
                pfd::icon::info);

    // Message box with busy loop
    auto m = pfd::message("Personal Message",
                          "You are an amazing person, don’t let anyone make you think otherwise.",
                          pfd::buttons::yes_no_cancel,
                          pfd::icon::warning);
    while (!m.ready(1000))
        std::cout << "Waited 1 second for user input..." << std::endl;
    std::cout << "User pressed button " << m.result() << std::endl;

    // File open
    pfd::open_file("Choose a file",
                   "/tmp/",
                   "Text Files Only | *.txt *.text", true);
}

