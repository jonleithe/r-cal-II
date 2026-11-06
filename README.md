# Calendar framework preview

A standalone ncurses calendar for working on the layout. Notes, tasks, events,
work statuses, and persistence are commented out until their dependencies return.
The header contains a minimal `rcal::Date` type for this preview.

Requires CMake 3.16 or newer, a C++17 compiler, a supported build tool (such as
Make or Ninja), and the ncurses wide-character development library
(`libncurses-dev` on Debian/Ubuntu). Run in an interactive terminal:

```sh
./build.sh debug
./build/debug/r-cal
```

Use `./build.sh release` for an optimized build in `build/release`, or
`./build.sh all` to build both configurations. With no argument, the script
defaults to `all`. It can be invoked from any working directory.

Optionally load the existing key=value configuration format with
`./build/debug/r-cal path/to/config`.

- Left/Right: previous/next day
- Page Up/Page Down: move by one visible page
- Home or n: today
- q, Q, or Escape: quit

The viewport adapts to terminal resizing. Use `cmake --build build/debug --target clean`
to remove compiled output.
