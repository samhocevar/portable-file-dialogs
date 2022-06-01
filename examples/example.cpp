//
//  Portable File Dialogs
//
//  Copyright © 2018–2022 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#include "portable-file-dialogs.h"

#include <iostream>

void test_notify();
void test_message();
void test_select_folder();
void test_open_file();
void test_save_file();

int main()
{
    // Check that a backend is available
    if (!pfd::settings::available())
    {
        std::cout << "Portable File Dialogs are not available on this platform.\n";
        return 1;
    }

    // Set verbosity to true
    pfd::settings::verbose(true);

    test_notify();
    test_message();
    test_select_folder();
    test_open_file();
    test_save_file();
}

void test_notify()
{
    // Notification
    pfd::notify("Important Notification",
                "This is ' a message, pay \" attention \\ to it!",
                pfd::icon::info);

}

void test_message()
{
    // Message box with nice message
    auto m = pfd::message("Personal Message",
                          "You are an amazing person, don’t let anyone make you think otherwise.",
                          pfd::choice::yes_no_cancel,
                          pfd::icon::warning);

    // Optional: do something while waiting for user action
    for (int i = 0; i < 10 && !m.ready(1000); ++i)
        std::cout << "Waited 1 second for user input...\n";

    // Do something according to the selected button
    switch (m.result())
    {
        case pfd::button::yes: std::cout << "User agreed.\n"; break;
        case pfd::button::no: std::cout << "User disagreed.\n"; break;
        case pfd::button::cancel: std::cout << "User freaked out.\n"; break;
        default: break; // Should not happen
    }
}

void test_select_folder()
{
    // Directory selection
    auto dir = pfd::select_folder("Select any directory", pfd::path::home()).result();
    std::cout << "Selected dir: " << dir << "\n";
}

void test_open_file()
{
    // File open
    auto f = pfd::open_file("Choose files to read", pfd::path::home(),
                            { "Text Files (.txt .text)", "*.txt *.text",
                              "All Files", "*" },
                            pfd::opt::multiselect);
    std::cout << "Selected files:";
    for (auto const &name : f.result())
        std::cout << " " + name;
    std::cout << "\n";
}

void test_save_file()
{
    // File save
    auto f = pfd::save_file("Choose file to save",
                            pfd::path::home() + pfd::path::separator() + "readme.txt",
                            { "Text Files (.txt .text)", "*.txt *.text" },
                            pfd::opt::force_overwrite);
    std::cout << "Selected file: " << f.result() << "\n";
}

// Unused function that just tests the whole API
void test_api()
{
    // pfd::settings
    pfd::settings::verbose(true);
    pfd::settings::rescan();

    // pfd::notify
    pfd::notify("", "");
    pfd::notify("", "", pfd::icon::info);
    pfd::notify("", "", pfd::icon::warning);
    pfd::notify("", "", pfd::icon::error);
    pfd::notify("", "", pfd::icon::question);

    pfd::notify a("", "");
    (void)a.ready();
    (void)a.ready(42);

    // pfd::message
    pfd::message("", "");
    pfd::message("", "", pfd::choice::ok);
    pfd::message("", "", pfd::choice::ok_cancel);
    pfd::message("", "", pfd::choice::yes_no);
    pfd::message("", "", pfd::choice::yes_no_cancel);
    pfd::message("", "", pfd::choice::retry_cancel);
    pfd::message("", "", pfd::choice::abort_retry_ignore);
    pfd::message("", "", pfd::choice::ok, pfd::icon::info);
    pfd::message("", "", pfd::choice::ok, pfd::icon::warning);
    pfd::message("", "", pfd::choice::ok, pfd::icon::error);
    pfd::message("", "", pfd::choice::ok, pfd::icon::question);

    pfd::message b("", "");
    (void)b.ready();
    (void)b.ready(42);
    (void)b.result();
}

