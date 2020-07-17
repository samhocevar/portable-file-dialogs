## General documentation

### Use as header-only library

Just include the main header file wherever needed:

```cpp
#include portable-file-dialogs.h

/* ... */

    pfd::message::message("Hello", "This is a test");

/* ... */
```

### Use as a single-file library

Defining the `PFD_SKIP_IMPLEMENTATION` macro before including `portable-file-dialogs.h` will
skip all the implementation code and reduce compilation times. You still need to include the
header without the macro at least once, typically in a `pfd-impl.cpp` file.

```cpp
// In pfd-impl.cpp
#include portable-file-dialogs.h
```

```cpp
// In all other files
#define PFD_SKIP_IMPLEMENTATION 1
#include portable-file-dialogs.h
```

### Settings

The library can be configured through the `pfd::settings` class.

```cpp
bool pfd::settings::available();
void pfd::settings::verbose(bool value);
void pfd::settings::rescan();
```

The return value of `pfd::settings::available()` indicates whether a suitable dialog backend (such
as Zenity or KDialog on Linux) has been found. If not found, the library will not work and all
dialog invocations will be no-ops.

Calling `pfd::settings::rescan()` will force a rescan of available backends. This may change the
result of `pfd::settings::available()` if a backend was installed on the system in the meantime.
This is probably only useful for debugging purposes.

Calling `pfd::settings::verbose(true)` may help debug the library. It will output debug information
to `std::cout` about some operations being performed.

