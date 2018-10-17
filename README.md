# Portable File Dialogs

A free C++11 file dialog library.

  * works on Linux, Windows.
  * single-header
  * asynchronous (does not block the rest of your program!)

Similar to [Tiny File Dialogs](https://sourceforge.net/projects/tinyfiledialogs/) but I like it better.

## Status

This is still experimental and nearly not as feature-complete as
[Tiny File Dialogs](https://sourceforge.net/projects/tinyfiledialogs/),
but for once it seemed more constructive to start a project from scratch
than try to fix its almost 1200 unchecked `strcat` or `strcpy` calls,
lack of proper shell escaping, and synchronous architecture.

The currently available backends are:

  * Win32 API (all known versions of Windows)
  * GNOME desktop (using [Zenity](https://en.wikipedia.org/wiki/Zenity) or its clones Matedialog and Qarma)
  * KDE desktop (using [KDialog](https://github.com/KDE/kdialog))

## Documentation

  * [Message Box API](https://github.com/samhocevar/portable-file-dialogs/issues/1)
  * [Notification API](https://github.com/samhocevar/portable-file-dialogs/issues/2)

## Screenshots

Windows 10:
![warning-win32](https://user-images.githubusercontent.com/245089/47136607-76919a00-d2b4-11e8-8f42-e2d62c4f9570.png)

Linux (GNOME desktop):
![warning-gnome](https://user-images.githubusercontent.com/245089/47136608-772a3080-d2b4-11e8-9e1d-60a7e743e908.png)

