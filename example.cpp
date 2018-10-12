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

int main()
{
    /* Notification */
    pfd::notify("Important notification",
                 "This is a notification, pay attention to it!",
                 pfd::icon::question);

    /* Message box */
    pfd::message("Personal message",
                 "You are a wonderful person, don’t let anyone let you think otherwise.",
                 pfd::buttons::ok_cancel,
                 pfd::icon::question);
}

