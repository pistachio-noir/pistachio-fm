# What to test by hand

The automated checks cover the backend (`build/tst_backend`, 39 checks) and a mouse-and-keyboard run on a made-up
home folder (`build/pfm-shots OUTDIR`, 23 checks). This list is what they can't see: your real desktop, real
files, real apps. Run the app with `build/pistachio-fm` (or `build/pistachio-fm ~/Downloads`). Compare it with
`design/pistachio-file-manager/html/*.html`.

## Looks

- [ + ] Manrope everywhere; the trash dialog's paths in a mono font (JetBrains Mono here falls back to "JetBrainsMono Nerd Font")
- [ + ] Window corners, border and shadow: they come from Hyprland, not the app (the boards have 28 px corners and a 1 px #39412A border)
- [ + ] Hover on tiles, rows, sidebar entries, toolbar buttons, the sort chip and menu entries
- [ + ] Resize the window: the grid reflows (tiles never under 160 px), a long path keeps its end in view, list columns stay lined up
- [ + ] A long file name ends in "…" in the grid and the list

## Browsing

- [ + ] Every sidebar place and device; the green chip follows (in Documents → Documents; in ~/Projects → Home)
- [ + ] Breadcrumbs are links; Ctrl+L (or a click on the path bar's empty end) takes `~/Downloads`, `/etc`, `..`, a typo (the problem shows bottom left)
- [ + ] Back, forward and up: the buttons, Alt+Left / Alt+Right / Alt+Up, Backspace, the mouse's side buttons
- [ + ] Going up selects the folder you came from
- [ + ] Double-click a folder; double-click a file: it opens in its app (whatever xdg-open picks)
- [ + ] Hidden files: Ctrl+H, or right-click empty 1space → Show hidden files
- [ + ] A huge folder (`/usr/lib`, `/usr/share/icons/*`) stays smooth; "Loading…" shows only for slow ones
- [ + ] A folder you can't read (`/root`) says so
- [ + ] Live updates: `touch ~/Documents/x` in a terminal → it appears, and the scroll position stays. Delete the folder you're in → the app moves to its parent
- [ + ] A USB stick (mounted under /run/media) shows under Devices within ~4 s and goes when unmounted
- [ ] The stick's eject button (at the end of its row) unmounts it: it stays under Devices, without the button; if
  you were in it, you're taken Home. With a file of it open elsewhere: "The drive is in use"
- [ ] Click the unmounted stick: it mounts and opens. A stick without a label shows as "32 GB drive"
- [ ] A drive udisks wants a password for (one in /etc/fstab under /mnt): the shell's polkit dialog asks

## Selecting and keys

- [ + ] Click, Ctrl+click, Shift+click, Ctrl+A, Escape, a click on empty space
- [ + ] Arrows in the grid and the list, Shift+arrows, Home/End, PageUp/PageDown, Enter
- [ + ] Typing over the files searches; Ctrl+F; Escape clears; Enter or Down goes back to the results
- [ + ] Sorting: the "Sorted by" chip menu, and the list's column names
- [ + ] Ctrl+1 / Ctrl+2 switch grid and list

## Menus and dialogs

- [ ] Right-click a file, several files, empty space; near the window's edges the menu stays inside; arrows + Enter work in it
- [ ] Properties (Alt+Enter, or the menu with one item selected): a file, a folder (its size fills in), a link (where it
  points), something in the trash; "Created" is left out where the drive doesn't record it
- [ ] Rename (F2): the extension isn't selected; a taken name says so; Enter and Escape
- [ ] New folder (Ctrl+Shift+N, or right-click empty space)
- [ ] Ctrl+C, then Ctrl+V in another folder: the copies land selected; the same folder gives "name (2)"; a taken name keeps both
- [ ] Ctrl+X fades the items; Ctrl+V in another folder moves them, and they stop being faded
- [ ] Copy in Nautilus, paste here, and the other way round (cut too); Ctrl+V into a terminal gives the paths
- [ ] Paste from the menu on empty space (it's there only when the clipboard holds files)
- [ ] A big copy (a few GB, or to a USB stick): the status line counts it up; the window stays usable meanwhile
- [ ] A folder into itself, into the trash, onto a full or read-only stick: a line says why, nothing half-copied stays
- [ ] Ctrl+Z after each of: paste (copy → the copies go to the trash; cut → back where they were), rename, new folder
  (stays if something went into it), move to trash, restore, compress. The status line says what was undone
- [ ] Drag a file onto a folder tile or row, a sidebar place, empty space: it moves (the target lights up while over it);
  onto a USB stick it copies; hold Ctrl while dropping to copy, Shift to move. Ctrl+Z takes a drop back
- [ ] Select several, press on one of them and drag: all of them go; a click on one without dragging picks just it
- [ ] Drag onto Trash in the sidebar: moved to the trash. Nothing can be dragged out of the trash (Restore does that)
- [ ] Drag from Nautilus or a browser's downloads into a folder here, and from here into Nautilus, a terminal (the path
  is typed), a browser's upload field; the other app keeps its files (a drop here never asks it to delete them)
- [ ] Ctrl+C / Ctrl+V / Ctrl+Z in the search field and the path bar work on their text, not the files
- [ ] Copy path, then paste it in a terminal (on Wayland the copy lives as long as the app does, unless a clipboard manager keeps it)
- [ ] Compress one file, a folder, several files: the .zip appears, selected; a second time gives "… (2).zip"
- [ ] Move to trash (Delete): the dialog lists the items; Enter confirms; they're in `~/.local/share/Trash/files`
- [ ] In Trash: Restore (also when the original folder is gone: it comes back), Delete forever (Shift+Delete too), Empty trash (right-click empty space). Those two start on Cancel
- [ ] Trashing from a USB stick (it goes to the stick's own `.Trash-1000`)

## Fitting in with the shell (optional, reversible)

- [ ] `cmake --install build --prefix ~/.local`, then `xdg-mime default pistachio-fm.desktop inode/directory` to make it the
  folder app (the shell's "Show in file manager" and the launcher's folders use xdg-open). Back with
  `xdg-mime default org.gnome.Nautilus.desktop inode/directory`
- [ ] "Pistachio FM" in the shell's launcher opens the app (after `cmake --install`)
- [ ] Hyprland window rules match class `pistachio-fm`

## Not built yet

A picture under the cursor while dragging (Hyprland shows only the cursor), dropping on breadcrumbs, scrolling or
opening folders while dragging over them, thumbnails, search in subfolders, tabs, encrypted drives (udiskie would have to ask for the password), powering a stick off after unmounting, properties of several items at once or of the folder you're in, changing
permissions, redo, a "Replace?" question when a pasted name is taken (both are kept), cancelling a paste, "Open with…".
