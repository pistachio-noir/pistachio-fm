# Pistachio FM: missing and limited functionality

Reviewed on 12 November 2026 against the local source and official Nautilus/GNOME Files and Dolphin documentation.

This is a feature audit, not a detailed implementation plan. The owner subsequently authorized continuing implementation with Codex in a new session; see `CODEX-HANDOFF.md`. Findings come from reading `CLAUDE.md`, `README.md`, `TESTING.md`, design documentation, C++ backend, QML UI, and relevant tests. No build or interactive verification was performed for this audit. Competitor features can depend on installed helpers, indexing services, and distribution packaging.

## Implementation update — 12 November 2026

Recursive filename search is now implemented alongside the fast local filter. Click the search icon to
change scope; results show relative paths. Scans run in the existing worker pool and stale scans stop
on query/navigation changes. Hidden subtrees follow Ctrl+H; directory symlinks are not traversed.
Result parents are watched, but new matches elsewhere need F5. Unreadable subfolders are skipped;
there is no index or content search. Nested-result rename validation now uses the actual parent. Compression preserves relative source paths;
recursive operations process selected descendants only once when their parent is also selected.

Verification: build and qmllint passed; backend 72 passed with `LC_ALL=en_US.UTF-8`; the offscreen
UI/screenshot runner passed all checks. Grid and recursive-search PNGs were visually inspected.
See CODEX-HANDOFF.md for commands, observed warnings, and remaining manual checks.

Quick preview was subsequently implemented at the owner's request: Space opens one selected file
from the focused pane. Read-only literal text (UTF-8/BOM Unicode, first 128 KiB) and Qt-supported images
are covered; Space/Escape close. The owner's later scope reply added PDF: existing Poppler helpers
render one page at a time with count/navigation, cancellation and temporary-image cleanup. Missing
Poppler, damaged/password-protected PDFs show errors. Media playback and office rendering remain
unsupported. No new Qt libraries were added; PDF preview requires the Poppler executables. New backend tests cover decoding/limits/errors/special files/cancellation;
UI tests cover opening/closing, contents, images with escaped names, unsupported formats, and text input.
Offscreen shots passed; text/image, both PDF pages and keys-card PNGs were inspected.
PDF backend coverage includes real page rendering/colours, boundary guards, cache removal, missing
helpers, damaged PDFs and killing an active helper on cancellation.

Settings follow-up: a toolbar button/Ctrl+comma opens common MIME types plus full MIME search.
The panel reads effective defaults with xdg-mime, offers installed app choices, and writes only on Apply,
then verifies the effective result. It does not launch files or change defaults merely by browsing.

## Already implemented

Do not treat these as missing: grid/list views; tabs; per-tab split panes; navigation history; keyboard selection and box selection; live folder updates; hidden files; copy/cut/paste with Nautilus/Dolphin clipboard formats; cross-application file dragging; background transfers with progress and Stop; ten-step undo; trash, restore and permanent deletion; per-drive trash; GTK-shared bookmarks; image thumbnails; ZIP creation and archive extraction; Open With and MIME defaults; external terminal launch; removable-drive mounting/unmounting.

## Main gaps compared with Nautilus and Dolphin

| Feature | Current Pistachio limitation | Comparison and practical impact |
| --- | --- | --- |
| Recursive search | Implemented: toggle the search icon; local filtering remains the default. New matches outside watched result parents need F5; unreadable subfolders are skipped. | Basic filename traversal is covered; indexed and advanced search remain out of scope. |
| Advanced search | No content search, date/type filters, index integration, or saved searches. | Nautilus offers date/type/content search. Dolphin additionally documents saved searches and metadata filters. |
| Transfer conflicts | Automatically keeps both with a numbered name. No Replace, Skip, Merge, or apply-to-all choice. | Nautilus has a conflict dialog. Updating an existing folder tree in Pistachio produces another folder instead of merging it. |
| Batch rename | Rename requires exactly one selected item. | Both competitors support batch rename; Nautilus offers templates, numbering, find/replace, and previews. |
| Remote locations | No native SMB, SSH/SFTP, FTP, or WebDAV connection or authentication workflow. Remote GTK bookmarks are retained but hidden. | Nautilus and KDE support network locations. Externally mounted storage remains usable through a local path. |
| Permission editing | Permissions and owner are displayed as text. No permission, group, or executable-bit controls. | Nautilus offers permission editing, including applying permissions to enclosed files. |
| Rich previews | Space opens text, image and paged PDF previews. Image/PDF zoom and panning are implemented; no PDF/video thumbnails, media playback or office-document rendering. | Nautilus has a document/media preview workflow; Dolphin has previews and an information panel, subject to installed components. |
| View preferences | No reverse sort, zoom, grouping, configurable columns, or stored per-folder display preferences. Grid/list mode is shared across panes. | Dolphin supports extensive view controls and remembers folder display settings. |
| Tree navigation | No folder-tree sidebar or expandable directory rows. | Dolphin provides a Folders panel and expandable folders in Details view. |
| Link creation | Can preserve existing symbolic links and show their targets, but cannot create a link through the UI. | Dolphin supports creating symbolic links through drag and drop. |
| Embedded terminal | Only launches an external terminal. | Dolphin also provides an integrated terminal panel. |

### Source locations

- `src/FolderModel.cpp`: `listFolder`, `startListing`, `resolve`, `rebuild`, and `lessThan`; local listing, recursive name search, local paths, substring filtering, and fixed sort directions.
- `qml/Main.qml`: `askRename`, `showProperties`, `itemEntries`, `backgroundEntries`, and `openSortMenu`; exposes the available user actions and single-selection restrictions.
- `src/Transfer.cpp`: `uniqueName` and `run`; always resolves destination collisions by keeping both.
- `qml/PropertiesDialog.qml` and `src/FileOps.cpp`: read-only permissions and single-item properties.
- `src/Apps.cpp`: `ThumbnailProvider`; Qt-readable pictures, round crop, no on-disk thumbnail cache.
- `src/Places.cpp`: `bookmarkPath` rejects remote URLs; `unmountedDrives` excludes locked LUKS volumes.

## Additional confirmed gaps

### Selection and properties

- No Properties for multiple selected items, including combined recursive size/count.
- No Properties for the current folder when nothing is selected.
- No invert-selection or pattern-selection action.

### Drag and drop

- No drops onto breadcrumb segments.
- No opening a folder by hovering over it during a file drag.
- No scrolling while dragging files or while extending a selection rectangle.
- No file thumbnail under the drag pointer.
- Existing copy/move modifier handling has a documented Wayland limitation: modifiers on incoming drags depend on the window receiving keyboard state. Do not promise identical behavior for every external drag without testing.

### Tabs and windows

- No tab reordering, detaching into another window, or reopening a closed tab.
- No overflow scrolling in the tab strip; its plain `Row` can exceed the available width.
- No in-app New Window action or shortcut. Launching another process can still open another window.
- No location-field path completion.
- No session restoration. This is explicitly a design decision, not an accidentally unfinished feature.

### Archives and new files

- Compress always creates ZIP; no format, output-name, or destination chooser.
- No archive-content browser or selective extraction.
- No password prompt for encrypted archives.
- New File creates an empty file; no document-template menu.
- No pasting clipboard text or images into a newly created file.

### Devices and trash

- Locked `crypto_LUKS` volumes are explicitly excluded from unmounted-device discovery; no unlock/lock UI.
- Unmount does not power off the drive.
- No dedicated phone/MTP browsing backend.
- The sidebar Trash opens the home trash, not an aggregate of every mounted volume's trash. A drive's trash can be opened by its local path.

### Operations and history

- Undo is limited to ten steps; no redo.
- No per-job operation list, individual cancellation, pause/resume, speed, ETA, or persistent error history.
- Stop applies to transfers. Archive subprocesses do not participate in that cancellation mechanism.
- Closing Properties does not cancel its background recursive size calculation.
- The initial recursive transfer survey has no stop check; cancellation can be delayed while surveying a large tree.

### Desktop integration and metadata

- No Recent, Starred, tags, ratings, or comments feature.
- No user-script/service-menu or version-control integration.
- No `org.freedesktop.FileManager1` D-Bus service in the current implementation; opening a path through the command line is supported separately.
- `Apps.cpp` explicitly skips desktop-specific MIME association lists and Removed Associations, so default-app resolution can differ from other desktop applications.
- Desktop-entry `%i`, `%c`, and `%k` fields are dropped; selected files are launched individually rather than grouped into a multi-file invocation.
- The transfer backend preserves basic permission modes and timestamps but has no explicit preservation of extended attributes, ACLs, or hard-link relationships. Treat this as a copy-fidelity limitation requiring focused investigation, not as an observed data-loss incident.
- UI strings are hard-coded English; no translation infrastructure was found.

## Deliberate choices to preserve unless the owner changes them

`CLAUDE.md` is authoritative. In particular:

- QML UI and C++/Qt backend are settled choices.
- The design boards are the visual source of truth.
- View mode is shared across panes.
- Tabs are intentionally not restored on startup.
- Name conflicts currently keep both; replacing this behavior needs a concrete design decision.
- Open With starts apps directly, avoiding the portal chooser for documented reasons.
- Window decoration belongs to Hyprland; never edit its configuration.

## Suggested priorities

This is the recommended order for the next session's implementation work, subject to newer owner instructions. Optional features and changes to explicit design decisions still need a scope decision:

1. **Done:** recursive filename search, retaining the current fast folder filter.
2. Transfer conflict choices: Replace / Skip / Keep Both, then folder merging.
3. Multi-item/current-folder Properties and permission editing.
4. Batch rename with a preview.
5. Reverse sorting, zoom, path completion, and remembered view preferences.
6. Extend the implemented text/image/PDF quick preview with media support; PDF/video thumbnails.
7. Drag autoscroll, breadcrumb drops, and tab overflow/reordering.
8. Native network and phone access if needed for the owner's workflows.

Copy fidelity and responsive cancellation deserve attention alongside new functionality. Embedded terminals, tags, and version-control integration are optional Dolphin-style enhancements, not prerequisites for this file manager's intended scope.

## Documentation discrepancy

`TESTING.md` lists cancelling a paste under "Not built yet", but `FileOps::stop`, the status-bar Stop chip, and the `stopPaste` backend test exist. That stale entry has now been removed. Stop's existence does not imply cancellation of archives or every preparatory operation.

## Official comparison sources

- [GNOME Files search](https://help.gnome.org/gnome-help/files-search.html)
- [GNOME Files batch rename](https://help.gnome.org/gnome-help/files-rename-multiple.html)
- [GNOME Files network locations](https://help.gnome.org/gnome-help/nautilus-connect.html)
- [GNOME Files permissions](https://help.gnome.org/gnome-help/nautilus-file-properties-permissions.html)
- [GNOME Files previews](https://help.gnome.org/gnome-help/files-preview.html)
- [Nautilus conflict-dialog source](https://github.com/GNOME/nautilus/blob/main/src/nautilus-file-conflict-dialog.c)
- [Dolphin search and batch rename](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/quick-tips.html)
- [Dolphin view controls and link creation](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/dolphin-view.html)
- [Dolphin remembered folder display settings](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/view-properties.html)
- [Dolphin preview configuration](https://docs.kde.org/trunk_kf6/en/dolphin/dolphin/configuring-dolphin.html)
- [Dolphin overview and integrated terminal](https://apps.kde.org/en-gb/dolphin/)
- [KDE network-folder integration](https://docs.kde.org/trunk_kf6/en/plasma-desktop/knetattach/knetattach.pdf)

## Starter prompt for a new session

```text
You're continuing pistachio-fm from our previous Codex session. We reviewed
missing features against Nautilus and Dolphin; now I want you to build the
improvements in this session.

Read CODEX-HANDOFF.md fully, then CLAUDE.md, README.md, TESTING.md, and
FEATURE-GAPS.md. Follow the workflow in handoff section 3b: verify the source,
respect existing decisions and boards, keep scope small, test actual changes,
update docs, and report honestly.

The audit was static: no build or interactive tests were run, and no application
code was changed. Verify the findings before implementing them.

Get oriented using src/FolderModel.cpp, src/FileOps.cpp, src/Transfer.cpp,
src/Places.cpp, and qml/Main.qml. Check the current git diff before editing.
Recursive filename search is implemented, with the fast local filter preserved.
Read its limits and tests before selecting the next increment from the backlog.
State a short plan, then implement and verify the next authorized improvement. Ask only about genuinely unresolved product decisions; preserve
explicit design choices and don't add every optional competitor feature.

Update FEATURE-GAPS.md and CODEX-HANDOFF.md with completed work, verification,
and remaining tasks so another Codex session can continue.
```
