# Portable File Dialogs

A free C++ file dialog library.

  * Works on Linux, Windows.
  * single-header
  * asynchronous (does not block the rest of your program)

Similar to [Tiny File Dialogs](https://sourceforge.net/projects/tinyfiledialogs/) but I like it better.

## Samples

Notification:

```cpp
pfd::notify("Important Notification",
            "This is a message, pay attention to it!",
            pfd::icon::info);
```

Asynchronous yes/no dialog:

```cpp
auto m = pfd::message("Message", "Do you agree?",
                      pfd::choice::yes_no_cancel,
                      pfd::icon::warning);

// Do something while waiting for user action
while (!m.ready(1000))
    std::cout << "Waited 1 second for user input...\n";

// Do something according to the selected button
switch (m.result())
{
    case pfd::button::yes: std::cout << "User agreed.\n"; break;
    case pfd::button::no: std::cout << "User disagreed.\n"; break;
    case pfd::button::cancel: std::cout << "User freaked out.\n"; break;
}
```

