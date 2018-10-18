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

## Screenshots (Windows 10)

![warning-win32](https://user-images.githubusercontent.com/245089/47136607-76919a00-d2b4-11e8-8f42-e2d62c4f9570.png)
![notify-win32](https://user-images.githubusercontent.com/245089/47142453-2ff76c00-d2c3-11e8-871a-1a110ac91eb2.png)

## Screenshots (Linux, GNOME desktop)

![warning-gnome](https://user-images.githubusercontent.com/245089/47136608-772a3080-d2b4-11e8-9e1d-60a7e743e908.png)
![notify-gnome](https://user-images.githubusercontent.com/245089/47142455-30900280-d2c3-11e8-8b76-ea16c7e502d4.png)

## Screenshots (Linux, KDE desktop)

![warning-kde](https://user-images.githubusercontent.com/245089/47149255-4dcccd00-d2d3-11e8-84c9-f85612784680.png)
![notify-kde](https://user-images.githubusercontent.com/245089/47149206-27a72d00-d2d3-11e8-8f1b-96e462f08c2b.png)

