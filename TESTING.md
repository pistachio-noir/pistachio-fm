# What to test by hand

The automated checks cover the backend (`build/tst_backend`) and a mouse-and-keyboard run on a made-up
home folder (`build/pfm-shots OUTDIR`). This list is what they can't see: your real desktop, real
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
- [ ] Click the search icon to include subfolders: duplicate filenames show their relative paths in grid/list;
  opening a file or folder, rename, trash, copy and undo target the actual result. Tab to the scope button and
  press Space or Enter to toggle it. Hidden subfolders follow Ctrl+H; directory symlinks are not traversed.
- [ ] Search a large tree; edit the query, clear it or navigate away while scanning: stale results never return.
  Unreadable subfolders are skipped. F5 finds new matches in subfolders that had no previous results.
- [ + ] Sorting: the "Sorted by" chip menu, and the list's column names
- [ + ] Ctrl+1 / Ctrl+2 switch grid and list

## Quick preview (Space)

- [ ] Select a text file or source file, Space: readable literal text (including `<b>`, without rendering HTML).
  Unicode with a BOM, an empty file and a file larger than 128 KiB: sensible content/notice, responsive UI.
- [ ] Select an image with spaces, `#` or `%` in its name: it fits the dialog with the right orientation.
  A broken image shows an error. No crop to a round thumbnail.
- [ ] Space again, Escape, Close and a click outside all close the preview; file selection remains and
  keyboard navigation returns to the files. Closing while loading never reopens an obsolete preview.
- [ ] Scroll long text, resize the window, select/copy text. Preview never modifies the source file.
- [ ] Spaces in search/path fields remain text input. No selection, several selected files or a folder:
  no arbitrary preview and no space appended to the file filter.
- [ ] Split panes and recursive-search results: preview the file selected in the active pane.
  Media/Office/other unsupported types show a clear fallback.
- [ ] PDF with several pages: Space shows page 1 and the count; buttons and Left/Right change pages;
  previous/next disable at document boundaries. Check a rotated/scanned PDF and a name containing `#`.
- [ ] Images/PDF: −/+ buttons, +/− keys and Ctrl+wheel zoom; drag/scroll pans the enlarged content.
  Fit or 0 resets zoom and panning; zoom stays between fit and 4×. New files start fitted, PDF page
  changes preserve zoom. Resize while zoomed and check that controls and scrollbars stay usable.
- [ ] Close a PDF during rendering or change files: no stale image reappears and temporary page PNGs
  are removed. Missing Poppler, damaged/password-protected PDFs and overly slow helpers show errors.
  Text/images remain usable without Poppler; no source files are modified.

## Settings: default applications

- [ ] Toolbar Settings and Ctrl+comma open Default applications; Escape/Close/outside return focus to files.
- [ ] Popular types are listed; search `PDF`, `image/png`, or another MIME; Enter inspects the first match.
  A custom exact MIME can be inspected without a sample file. Type list keyboard arrows + Enter work.
- [ ] Current application agrees with `xdg-mime query default application/pdf` for PDF (likewise for
  the selected type); no default appears as None, an unknown app ID remains visible instead of a guessed name.
- [ ] Selecting a different app does not change the default; Apply writes, verifies, and reports the actual
  effective result without launching the application. Note the old app before a manual change and restore it.
- [ ] Recommended/Show all, app-list keyboard selection, Apply activation, and long names work.
- [ ] Quick type changes never display stale defaults; closing during Apply doesn't interrupt the save.
  A missing/failing/slow xdg-mime produces an error; a higher-priority override is not reported as success.

## Menus and dialogs

- [ ] Right-click a file, several files, empty space; near the window's edges the menu stays inside; arrows + Enter work in it
- [ ] Properties (Alt+Enter, or the menu with one item selected): a file, a folder (its size fills in), a link (where it
  points), something in the trash; "Created" is left out where the drive doesn't record it
- [ ] Open with… on a file: the type's apps with their icons, the default first and marked; Show all apps; a terminal
  app (Neovim) opens in kitty; Enter / double-click open
- [ ] A double-click on a .jpg opens it in Loupe straight away (no GTK "Open With…" chooser in between)
- [ ] "Always use" on: the next double-click opens it with that app; `xdg-mime query default text/markdown` agrees and
  the rest of `~/.config/mimeapps.list` is unchanged. Off: nothing changes
- [ ] Properties: "Opens with" and Change (None and Choose for a type nothing opens)
- [ ] Rename (F2): the extension isn't selected; a taken name says so; Enter and Escape
- [ ] New folder (Ctrl+Shift+N, or right-click empty space)
- [ ] New file (Ctrl+Alt+N, or right-click empty space): `notes.md`, `Makefile` (no extension), a taken name says so; Ctrl+Z
  removes it only while it's empty
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
- [ ] Extract here on a .zip, a .tar.gz, a .7z: a folder named after it, "(2)" the second time; Ctrl+Z trashes it; a
  broken archive says why and leaves nothing behind
- [ ] Right-click empty space, Open in terminal: your terminal opens in that folder
- [ ] A big paste (a few GB to a stick): Stop in the status bar stops it; the half-copied item is gone, the rest stays
- [ ] Pictures show in their badges (JPEG, PNG, WebP; a phone photo the right way up); RAW photos keep the glyph;
  a folder of a few thousand photos scrolls without stutter
- [ ] Drag a box on empty space in the grid and the list; Ctrl adds; a click on empty space clears
- [ ] Ctrl+D, "Add to bookmarks", and a folder dropped on the sidebar each add a bookmark; it's lit when you're in
  it; right-click removes it; Nautilus shows the same bookmarks, and one added there shows here within seconds
- [ ] Open with: the apps' real icons show (they never did before 7 Nov: the icon provider wasn't registered)
- [ ] Move to trash (Delete): the dialog lists the items; Enter confirms; they're in `~/.local/share/Trash/files`
- [ ] Tabs: Ctrl+T, the + and "Open in new tab"/middle-click on a folder; the row shows only with two or more; click,
  Ctrl+Tab and Ctrl+Shift+Tab switch, each keeps its folder, history and search; × , middle-click and Ctrl+W close
  (the window with the last)
- [ ] Split (F3): the new pane opens on the same folder; click a pane and the path bar, sidebar, status line and keys
  follow it; drag and paste between the panes; F3 again keeps the pane you're in
- [ ] Right-click a tab: Split view / Close split (it goes to that tab) and Close tab. Drag a tab onto the left or right
  third of the files: the tab's chip follows the pointer, that side lights, and on letting go its folder is the pane there (the tab on show splits) and
  the tab goes; dragged onto the middle, or the tab on show dragged, nothing happens
- [ ] The toolbar's keyboard button (and F1, ?) lists the keys; Escape, Close or a click outside closes it
- [ ] Desktop is the second place (under Home) and opens ~/Desktop
- [ ] Trash is the last of Places, in view even with a stick's partitions under Devices
- [ ] In Trash: Restore (also when the original folder is gone: it comes back), Delete forever (Shift+Delete too), Empty trash
  (the status bar chip, or right-click empty space; the chip hides when the trash is empty). Those two start on Cancel
- [ ] Properties of something in the trash: "Deleted from" and "Deleted" instead of its location
- [ ] A stick's own trash (Ctrl+H, then `.Trash-1000/files` on the stick): Empty trash empties that one, not the home trash
- [ ] Trashing from a USB stick (it goes to the stick's own `.Trash-1000`)

## Fitting in with the shell (optional, reversible)

- [ ] `cmake --install build --prefix ~/.local`, then `xdg-mime default pistachio-fm.desktop inode/directory` to make it the
  folder app (the shell's "Show in file manager" and the launcher's folders use xdg-open). Back with
  `xdg-mime default org.gnome.Nautilus.desktop inode/directory`
- [ ] "Pistachio FM" in the shell's launcher opens the app (after `cmake --install`)
- [ ] Hyprland window rules match class `pistachio-fm`

## Not built yet

A picture under the cursor while dragging (Hyprland shows only the cursor), dropping on breadcrumbs, scrolling or
opening folders while dragging over them, thumbnails of videos and PDFs, scrolling while drag-selecting, reordering tabs or pulling one out into a window of its own, encrypted drives (udiskie would have to ask for the password), powering a stick off after unmounting, properties of several items at once or of the folder you're in, changing
permissions, redo, a "Replace?" question when a pasted name is taken (both are kept).

## Automated run notes (12 November 2026)

The inherited `listing` test expects numeric collation; this environment's `LC_ALL=C.UTF-8` orders
`file10` before `file2`. Run the backend with `LC_ALL=en_US.UTF-8` for that expectation. The screenshot
fixtures include deliberately invalid images, which produce thumbnail warnings; inspect PASS/FAIL and
exit status separately. Real desktop and drive checks above still require manual testing.
