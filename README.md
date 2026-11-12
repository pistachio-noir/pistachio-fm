# Pistachio Files

A file manager in Pistachio Noir, to sit next to the Pistachio shell. QML for the screens, C++ (Qt 6) for
everything that touches files. The design is in `design/pistachio-file-manager/` (open `html/*.html` in a browser).

## Build and run

Needs Qt 6.9 or newer (`qt6-base`, `qt6-declarative`), CMake, Ninja, and `bsdtar` from libarchive for Compress and Extract here.
Fonts: Manrope and JetBrains Mono, as for the shell. PDF preview uses Poppler's `pdfinfo` and `pdftoppm`
(`poppler` on Arch, `poppler-utils` on Debian/Ubuntu); text/images still work without them.

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
The version, in the status bar's corner and `pistachio-fm --version`, is `1 · <short hash>`: every build reads
HEAD's short hash (`git rev-parse --short HEAD`), so it follows the repo; bump the major in `CMakeLists.txt`'s `project()`.

## Keys

| Keys | |
|---|---|
| Arrows, Shift+arrows, Home/End, PageUp/PageDown | move, extend the selection |
| Enter, double-click | open (folder here, file in its app) |
| Space | preview one selected file: text, image or PDF; Space / Escape closes |
| Backspace, Alt+Up | up a folder |
| Alt+Left / Alt+Right, mouse side buttons | back / forward |
| Alt+Home | home |
| Ctrl+L | type a path |
| Ctrl+, (or the Settings toolbar button) | default applications: common file types and MIME search |
| Ctrl+F, or just type | search this folder |
| Escape | clear the search, then the selection |
| Ctrl+A | select all |
| F2 | rename |
| Open with… (right-click a file) | pick the app; "Always use for <type>" makes it the desktop's default for that type (in `~/.config/mimeapps.list`, which xdg-open and the shell follow). Properties shows what opens a file now, with Change |
| Alt+Enter | properties: type, size (a folder's counted), location, created, modified, permissions |
| Ctrl+Shift+N | new folder |
| Ctrl+Alt+N | new file: empty, any name and extension ("New file.txt" to start; type over the name, edit the extension) |
| Ctrl+C / Ctrl+X / Ctrl+V | copy / cut / paste files (shared with other apps: Nautilus, Dolphin, a terminal gets the paths) |
| Drag and drop | onto a folder, a sidebar place or empty space, to and from other apps: a move on the same drive, a copy to another; hold Ctrl to copy, Shift to move. Onto Trash: move to trash |
| Ctrl+Z | undo the latest paste or drop, rename, new folder or file, move to trash, restore, compress or extract (up to 10) |
| Delete / Shift+Delete | move to trash / delete forever (both ask first) |
| Trash | the last of the sidebar's Places; in it, right-click an item to Restore or Delete forever, and the status bar's Empty trash empties it (it asks first). Properties says where an item was deleted from, and when |
| Ctrl+D | bookmark the folder you're in; also "Add to bookmarks" on a folder's right-click, or drop a folder on the sidebar (off a place). Bookmarks live in `~/.config/gtk-3.0/bookmarks`, shared with Nautilus; right-click one to remove it |
| Drag on empty space | a box: selects what it touches; with Ctrl, on top of the selection |
| Right-click an archive | Extract here: into a new folder named after it ("photos.tar.gz" → "photos/") |
| Right-click empty space | Open in terminal (`$TERMINAL`, or kitty) in that folder |
| Stop (status bar) | while a paste or drop shows its progress: stops it and the ones waiting; what's done stays |
| Ctrl+H | hidden files |
| Ctrl+1 / Ctrl+2 | grid / list |
| Ctrl+T / Ctrl+W | new tab (same folder) / close tab (the window with the last); middle-click a folder or "Open in new tab" opens it in one, middle-click a tab closes it |
| Ctrl+Tab / Ctrl+Shift+Tab | next / previous tab (Ctrl+PageDown / PageUp too) |
| F3 | split the tab in two panes side by side; again, back to one (keeping the pane you're in) |
| (mouse) | right-click a tab: Split view (Close split) or Close tab; drag a tab onto the left or right of the files to put its folder in that pane beside the tab on show, and the tab goes |
| F5, Ctrl+R | reload |
| Ctrl+Q | close the window |
| F1, ? (or the toolbar's keyboard button) | this list of keys, in the app |

Click the search icon to include subfolders (click again for the fast current-folder filter).
Results show paths relative to the folder being searched. Search matches filenames, ignores case,
respects Ctrl+H, and does not follow directory symlinks. Escape or the clear button restores the
root listing; opening another folder resets the scope. Recursive search is unavailable in Trash.
Results appear when the scan finishes; editing/clearing the query or navigating cancels an older scan.
Unreadable subfolders are skipped. Result folders are watched for changes; use F5 for new matches
in other subfolders. No content search or index is used.

Space over the file view opens a read-only preview of one selected file. Text (UTF-8 or Unicode
with a BOM) is displayed literally, up to the first 128 KiB, with a notice when truncated. Images
use Qt's installed image formats and fit inside the dialog. Empty files, read errors and unsupported
formats have explicit messages. PDFs render one page at a time (up to 1600 px), with page count,
Previous/Next buttons and Left/Right keys. Rendered pages are temporary and removed on page change
or closing; the source PDF is untouched. Rendering runs in the background, is cancelled on close,
and has a 10-second limit per helper. Missing Poppler, damaged and password-protected PDFs show
errors; there is no password prompt. Image/PDF previews zoom from fit to 4× using −/+, the
+/− keys or Ctrl+wheel; drag or scroll to pan. Fit or 0 resets the view. Each newly opened file
starts fitted; PDF page changes preserve zoom. Large images decode at up to 4096 px per dimension;
PDF pages retain the 1600 px rendering limit. Audio/video playback and office-document rendering are not included. Space inside search/path inputs still types a space; folders and multiple selections do
not open an arbitrary preview.

Settings opens with the toolbar's sliders button or Ctrl+comma. Popular MIME types are shown first;
search by type name or MIME, and enter a full custom MIME when needed. Select a type to read its
current desktop default through `xdg-mime`, choose a recommended app (or Show all apps), then Apply.
Selecting/browsing never writes defaults. Apply changes the desktop-wide association without launching
an app, then queries it again; errors or desktop-specific overrides are reported instead of claiming
success. Reads and writes are asynchronous with a five-second helper timeout. Closing the popup does
not cancel an Apply already requested. The panel requires `xdg-mime` from `xdg-utils`.

## Code

- `src/`: the backend, registered with QML as module `Pistachio.Files`
  - `FolderModel`: a folder's entries (listed off the GUI thread, kept live with a file watcher), selection,
    search filter, sort, back/forward history, the status line
  - `FileOps`: open, rename, new folder, trash/restore/delete (freedesktop.org trash), copy path, compress and extract,
    the clipboard (copy, cut, paste), drops and undo
  - `Apps`: installed apps (desktop entries), which open a file's type, the default for a type (`mimeapps.list`), starting one on a file
    or a terminal; the image providers for app icons and pictures' round thumbnails
  - `Transfer`: the copying and moving behind paste, drop and undo, on a worker thread, with its progress line
  - `Places`: the sidebar's places, bookmarks (the GTK bookmarks file) and devices (removable drives that aren't mounted, from `lsblk`), mounting and
    unmounting them with udiskie, breadcrumbs
  - `MimeSettings`: asynchronous xdg-mime query/save/readback, catalog and input validation
  - `Preview`: read-only preview loading, bounded text reads and stale-result cancellation
  - `Format`: "2.4 MB", "Oct 27", "14 items", file kinds
- `qml/`: the screens; `Theme.qml` holds every colour and size, `Icons.qml` the board's Lucide paths
- `tests/tst_backend.cpp`: the backend (`ctest --test-dir build`)
- `tests/shots.cpp`: `build/pfm-shots OUTDIR` runs the app on a made-up home laid out like the boards, saves
  screenshots of the boards' states and checks the mouse and keys (`QT_QPA_PLATFORM=offscreen` keeps it off
  the desktop)
- `TESTING.md`: what to try by hand
