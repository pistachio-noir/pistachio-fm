# Pistachio Files

A file manager for the owner's Pistachio shell (`~/Desktop/pistachio-shell`, a Quickshell config), in the same
Pistachio Noir look. `README.md` has the build, keys and code layout; `TESTING.md` the hand-test list.

## Decisions (already made)

- UI in QML, backend in C++ on Qt 6 (the owner's choice of QML; C++ over Rust because the shell runs on the same
  Qt 6 QML stack and Qt has the file-manager pieces). One static QML module, `Pistachio.Files`, built by
  `CMakeLists.txt`; the QML files are aliased to the module's root so they see each other and the singletons.
- The boards in `design/pistachio-file-manager/html/` are the visual source of truth (sizes, radii, colours).
  Their sample names, sizes and dates are made up.
- Deliberate departures from the boards: the window opens at 960 x 600 (the board's 516 cuts off Devices); the
  list's "Name" header lines up with the names (the board has it 12 px further right); the sidebar lights the place
  holding the folder most closely (the grid board shows Home lit while in Documents, the list board Documents).
  Copy, Cut and Paste are additions (owner's request, no board): Copy took the board's copy icon, so Copy path has
  Lucide's link; cut items fade to `Theme.cutOpacity`; a pasted name that's taken keeps both, as Restore does.
  Desktop is the second place, under Home (owner's request, 31 Oct: they go there often; not on the boards).
  Trash is the last of Places, not under Devices as on the boards, so drives plugged in never push it out of view;
  in a trash, an Empty trash chip (terracotta) sits next to the sort chip; Properties of a trashed item shows
  where it was deleted from and when (owner's choices, 31 Oct). Empty trash empties the trash folder you're in
  (home or a drive's), never another.
- Open with (owner's choices, 31 Oct; board `open-with.html`, drawn by Claude and approved): per file type (MIME),
  desktop-wide in `~/.config/mimeapps.list` (only that type's line in [Default Applications] changes; written
  whole with QSaveFile); the type's apps first, Show all for the rest; "Always use" starts on only when the type
  has no default; Properties has Opens with + Change. FM starts apps itself from Exec= (terminal apps in
  $TERMINAL or kitty, as the shell's launcher), not through gio.
- Tabs and split view (owner's choices, 1 Nov; board `tabs-split.html`, drawn by Claude and approved): Dolphin's
  model, each tab can split in two (F3); the tab row only from two tabs on; Ctrl+W closes the tab, the window with
  the last; tabs aren't restored on start. Main's `folderModel` is the folder of the pane you're in (`Pane` in a
  `TabPage`), so the toolbar, sidebar, menus, dialogs and status line work on it unchanged; the view mode is shared.
  Right-click a tab: Split view / Close split and Close tab; a tab dragged onto the left or right third of the files
  goes in the pane on that side of the tab on show (splitting it) and closes, its pane you were in if it was split;
  the side lights in overlay2 as a folder under a file drag, and a copy of the tab's chip (bg, overlay2 border) follows
  the pointer (owner's choices 2 Nov, no board). The drag is a mouse
  drag inside the window (TabBar's dragged/dropped), not a platform drag.
- From Nautilus (owner's picks 7 Nov, no boards: they reuse what's there): pictures show in the round kind badge
  (`image://thumbnail/`, cut round in C++; no on-disk cache); Extract here into a new folder named after the
  archive (bsdtar without -P, so nothing lands outside it; undo trashes the folder); Open in terminal on empty
  space; a Stop chip while a paste shows progress; Bookmarks under Places from `~/.config/gtk-3.0/bookmarks`
  (Nautilus' file, re-read on the 4 s poll; only that line changes); a box drag on empty space selects.
- The version (owner's request 7 Nov, no board): `v1 · <HEAD's short hash>` in subtext at the status bar's right corner,
  right of the sort chip; `cmake/version.cmake` reads the hash on every build into `build/version.h`.
- Recursive filename search (12 Nov, implementation from CODEX-HANDOFF.md; no new board): clicking the search
  icon toggles subfolder search; typing/Ctrl+F retains the fast local filter by default. Names in results show
  relative paths. No directory symlink traversal; hidden subtrees follow Ctrl+H. Navigation resets the scope;
  Trash does not offer it. Result parents are watched; F5 is needed for new matches elsewhere. No index or
  content search. Rename validation uses the result's actual parent; compression keeps relative source paths. Recursive
  file operations ignore descendants already covered by a selected real directory, never by a selected symlink.
- Quick preview (owner requested Space on 12 Nov; implementation defaults, no new board): one selected
  file in the focused file view opens a Pistachio Noir popup. Space/Escape close, text fields retain spaces.
  Text is read-only/plain (UTF-8 or BOM Unicode), capped at 128 KiB with notice, loaded in a worker with
  stale-result invalidation. Images fit the popup using Qt image support. Unsupported files/errors show
  explicit messages. Owner's later scope reply includes PDF: use installed Poppler (pdfinfo/pdftoppm),
  one page at a time up to 1600 px with count/previous/next and Left/Right. Workers kill helpers on close
  or after 10 seconds; page PNGs live in private temporary directories and are removed when replaced/closed.
  Zoom (owner's request, 12 Nov): images/PDF zoom from fit to 4× with buttons, +/− keys or Ctrl+wheel;
  drag/scroll pans, Fit/0 resets; each opened file starts fitted, PDF page changes retain zoom. Image
  decode dimensions are capped at 4096 px; PDF rendering remains capped at 1600 px.
  Missing Poppler, damaged/password-protected PDFs show errors. No password dialog, media or office renderer.
- Default applications settings (owner's request/scope 12 Nov): toolbar sliders button and Ctrl+comma;
  popular types plus search of any MIME, with custom exact types supported. Settings uses asynchronous
  xdg-mime queries for effective desktop defaults, recommended/all app choices from Apps.forMime/all,
  and explicit Apply with verification. No app launch or write on selection. Changes affect the desktop,
  with a five-second process limit; closing the popup preserves an Apply already in progress. Unknown
  app IDs and invalid MIME input are rejected. Existing Open with launch behavior is preserved.
- The keys card (toolbar's keyboard button, F1, ?; owner's request 1 Nov, no board) lists the keys from data in
  `KeysDialog.qml`, chips as the shell launcher's cheat sheet: keep it in step with Main's Shortcuts and README.
- Window corners, border and shadow are Hyprland's. Never edit the Hyprland config.

## Style rules (as the shell's)

- Colours only from `qml/Theme.qml`, which mirrors the shell's `src/Theme.qml`. No hex values elsewhere.
- Pistachio marks only what's active: the current place, selected tiles and rows (pistachio with bg-coloured
  text), the chosen sort. No accent borders, no large pistachio fills. Dangerous actions are terracotta text on overlay.
- Manrope 500–800 for the UI, JetBrains Mono for paths; numbers with tabular figures. Radius 28 panels, 20 tiles
  and menus, round buttons. One easing (OutCubic), 120 ms hovers, 200 ms fades.

## Working rules

1. Build `cmake --build build`; check with `QT_QPA_PLATFORM=offscreen build/tst_backend` and
   `QT_QPA_PLATFORM=offscreen build/pfm-shots <scratch dir>` (saves the boards' states as PNGs and prints
   PASS/FAIL), then look at the PNGs next to the boards. `cmake --build build --target all_qmllint` should stay quiet.
2. `pfm-shots` clicks fixed points of the 960 x 600 layout; move them when the layout changes. Devices lists the
   real drives plugged in (a USB stick moves Trash down, out of view), so it finds sidebar entries by path and
   scrolls to them, and stops if it lands anywhere else: it must never click around on a real drive.
3. Qt logs to journald when stderr isn't a terminal, so QML errors vanish from tool output: set
   `QT_FORCE_STDERR_LOGGING=1`.

## Gotchas found while building

- Import Qt Quick Controls with a prefix (`import QtQuick.Controls.Basic as Controls`): Qt 6.9 added
  `ContextMenu` and 6.10 `SearchField`, and an unprefixed import made them win over this module's own types of
  the same name ("Type cannot be created", "non-existent property onDone").
- Popups (`Controls.Popup`) whose closePolicy has CloseOnEscape hold the window's `Shortcut`s for as long as they're
  on screen, their fade-out included (Qt by design). The files take the focus back on `aboutToHide`.
- Offscreen (`QT_QPA_PLATFORM=offscreen`) `grabWindow`, a `Shape` (the icons) in a row cut by a ListView's clip
  draws past the clip; on Hyprland it's clipped correctly (checked with a minimal scene). Don't work around it in the app.
- `pfm-shots` points HOME at a temporary folder; fontconfig reads the user's fonts from HOME, so it loads the font
  database before moving HOME, or everything renders in the fallback font.
- The shell's launcher starts apps with the session's PATH, which lacks `~/.local/bin`, so `cmake --install` writes
  the app's full path into the installed desktop entry's Exec and TryExec (`data/pistachio-fm.desktop` keeps the
  bare name). The shell finds only hicolor icons (no Qt platform theme), so the app's own icon
  (`data/pistachio-fm.svg`, drawn like pistachio-notes') is installed to hicolor/scalable/apps.
- Wayland tells only the focused window that the clipboard changed, so `FileOps::paste` reads it again first.
- A Popup's `focus: true` focuses the popup, not its contentItem: Keys handlers on the content need `focus: true`
  there too (FileMenu's arrows and Enter did nothing until that).
- Drags are platform drags (`Drag.Automatic`), so this window and other apps take them alike. Qt Quick's drop event
  has no keyboard modifiers, so `FileOps::drop` asks `QGuiApplication` (Wayland only tells the focused window, so
  Ctrl/Shift work on drags that start here). Offscreen, Qt ends every drag at once without a drop: `pfm-shots`
  starts real drags with the mouse, then hands the window the drop itself, with that drag's data.
- Drives: `Places` lists removable drives' unmounted partitions from `lsblk -J -l` on the 4 s poll, and mounts and
  unmounts through `udiskie-mount` / `udiskie-umount` (udisks asks polkit, whose agent is the shell's: no pkexec).
  `tst_backend` tests them with stand-in scripts first on PATH, never a real drive.
- A popup opened from FileMenu loses the keyboard when the menu finishes closing (Qt hands the focus back to the
  files then), so dialogs take it in `onOpened`. Without that, Enter in Open with opened the file in its old app.
- moc gives up on a test file with raw string literals holding escaped quotes ("No relevant classes found", then
  "undefined reference to vtable"); write those strings with ordinary escapes.
- Offscreen Qt has no icon theme, so app icons only show on the real desktop (hicolor; Papirus isn't seen, no
  platform theme); the card falls back to the app's first letter.
- QML builds a default-constructible singleton with its constructor and never calls its `create()`: `Apps` takes
  a parent with no default, or its image providers (app icons, thumbnails) are never added.
- Offscreen, shader effects (`MultiEffect`, `RectangularShadow`) draw nothing, so the thumbnails are cut round in
  C++ (and shadows never show in pfm-shots' PNGs).
- Don't open files with `QDesktopServices::openUrl`: Qt 6.12 sends it through the desktop portal (OpenURI), whose
  GTK "Open With…" chooser keeps asking, with its own memory per app, whatever `mimeapps.list` says. FM starts
  the default itself (`Apps::openFiles`, the same launcher as Open with).
- `FolderModel` keys selection by path, so reloads keep it; small changes are applied as row inserts and removes
  (no reset), so the views keep their scroll position.
