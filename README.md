# Pistachio Files

A file manager in Pistachio Noir, to sit next to the Pistachio shell. QML for the screens, C++ (Qt 6) for
everything that touches files. The design is in `design/pistachio-file-manager/` (open `html/*.html` in a browser).

## Build and run

Needs Qt 6.9 or newer (`qt6-base`, `qt6-declarative`), CMake, Ninja, and `bsdtar` from libarchive for Compress.
Fonts: Manrope and JetBrains Mono, as for the shell.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
build/pistachio-fm                # home
build/pistachio-fm ~/Downloads    # a folder; a file opens its folder with it selected
```

To install it as `~/.local/bin/pistachio-fm` with a desktop entry ("Pistachio FM" in launchers):
`cmake --install build --prefix ~/.local`. The installed entry runs the app by its full path, so launchers whose
PATH lacks `~/.local/bin` (the Pistachio shell's does) start it too.
The window's class (Wayland app id) is `pistachio-fm`.

## Keys

| Keys | |
|---|---|
| Arrows, Shift+arrows, Home/End, PageUp/PageDown | move, extend the selection |
| Enter, double-click | open (folder here, file in its app) |
| Backspace, Alt+Up | up a folder |
| Alt+Left / Alt+Right, mouse side buttons | back / forward |
| Alt+Home | home |
| Ctrl+L | type a path |
| Ctrl+F, or just type | search this folder |
| Escape | clear the search, then the selection |
| Ctrl+A | select all |
| F2 | rename |
| Alt+Enter | properties: type, size (a folder's counted), location, created, modified, permissions |
| Ctrl+Shift+N | new folder |
| Ctrl+C / Ctrl+X / Ctrl+V | copy / cut / paste files (shared with other apps: Nautilus, Dolphin, a terminal gets the paths) |
| Drag and drop | onto a folder, a sidebar place or empty space, to and from other apps: a move on the same drive, a copy to another; hold Ctrl to copy, Shift to move. Onto Trash: move to trash |
| Ctrl+Z | undo the latest paste or drop, rename, new folder, move to trash, restore or compress (up to 10) |
| Delete / Shift+Delete | move to trash / delete forever (both ask first) |
| Ctrl+H | hidden files |
| Ctrl+1 / Ctrl+2 | grid / list |
| F5, Ctrl+R | reload |
| Ctrl+W, Ctrl+Q | close |

## Code

- `src/`: the backend, registered with QML as module `Pistachio.Files`
  - `FolderModel`: a folder's entries (listed off the GUI thread, kept live with a file watcher), selection,
    search filter, sort, back/forward history, the status line
  - `FileOps`: open, rename, new folder, trash/restore/delete (freedesktop.org trash), copy path, compress,
    the clipboard (copy, cut, paste), drops and undo
  - `Transfer`: the copying and moving behind paste, drop and undo, on a worker thread, with its progress line
  - `Places`: the sidebar's places and devices (removable drives that aren't mounted, from `lsblk`), mounting and
    unmounting them with udiskie, breadcrumbs
  - `Format`: "2.4 MB", "Oct 27", "14 items", file kinds
- `qml/`: the screens; `Theme.qml` holds every colour and size, `Icons.qml` the board's Lucide paths
- `tests/tst_backend.cpp`: the backend (`ctest --test-dir build`)
- `tests/shots.cpp`: `build/pfm-shots OUTDIR` runs the app on a made-up home laid out like the boards, saves
  screenshots of the boards' states and checks the mouse and keys (`QT_QPA_PLATFORM=offscreen` keeps it off
  the desktop)
- `TESTING.md`: what to try by hand
