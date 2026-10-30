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
  bare name). The shell finds only hicolor icons (no Qt platform theme), so `Icon=system-file-manager`, a Papirus
  icon, shows as a placeholder there.
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
- `FolderModel` keys selection by path, so reloads keep it; small changes are applied as row inserts and removes
  (no reset), so the views keep their scroll position.
