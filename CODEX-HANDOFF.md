# Handoff for Codex in the next session

Written by Codex on 12 November 2026. The owner will continue with Codex in a fresh session to implement improvements from the feature-gap audit. This is a Codex-to-Codex handoff.

## 1. Read first, in this order

1. `CLAUDE.md`: project rules apply to Codex too, despite the filename.
2. `README.md`: build, shortcuts, and code map.
3. `TESTING.md`: manual verification, known omissions, and the locale requirement for numeric-sort tests.
4. `FEATURE-GAPS.md`: findings, competitor sources, suggested priorities, and copyable starter prompt.

## 2. What the project is

A local file manager for the owner's Pistachio shell, with a Qt 6/C++ backend and QML UI. `src/` contains FolderModel (listing, filtering, sorting, selection), FileOps (actions/undo), Transfer (worker-thread copying/moving), Places (bookmarks/devices), Apps (launching/defaults/thumbnails), and Format. `qml/Main.qml` connects the toolbar, sidebar, tabs, panes, menus, and dialogs. Design boards live in `design/pistachio-file-manager/html/`. Tests are `tests/tst_backend.cpp` and `tests/shots.cpp`.

For implementation changes, follow CLAUDE.md's checks:

```sh
cmake --build build
QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/tst_backend
QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/pfm-shots /tmp/pfm-review-shots
cmake --build build --target all_qmllint
```

Inspect generated PNGs against the boards for visual changes. The screenshot runner uses a scratch home; follow the real-drive safety rules in CLAUDE.md.

## 3. Owner's working preferences

Keep the Pistachio Noir design and established choices. Colours belong in Theme.qml; the boards define visual styling. Do not edit Hyprland configuration. The owner wants implementation in the new Codex session, not further review only. Treat the audit as a backlog, not a requirement to add every competitor feature or reverse deliberate choices. No commit was requested.

## 3b. How the owner and Codex worked together

Preserve the workflow actually used in this review:

1. Read project instructions and docs, then check claims against backend and UI source.
2. Separate implemented features, actual omissions, deliberate choices, and recommendations.
3. Use official competitor documentation/source when making feature comparisons.
4. Keep the owner informed with concise, concrete updates.
5. Do not claim runtime verification from static inspection.
6. Record the findings and enough context for a new session to continue.

For implementation, follow project checks, use neighbouring code patterns, update affected docs, and report what was and was not tested. No detailed implementation design was agreed during this session.

## 4. Your task: implement improvements from the feature audit

The owner explicitly wants Codex to build improvements in the next session. Read FEATURE-GAPS.md and verify its findings against the current source. Recursive filename search, the default starting feature, is now implemented while preserving the existing fast current-folder filter. State a short implementation and verification plan, then proceed with the authorized work. Ask only for genuinely unresolved product decisions; do not add a mandatory approval stop just to restart the task.

Work in focused increments using the suggested priorities as a default order. Keep optional Dolphin-style enhancements and reversals of documented choices out of scope unless requested. Update the backlog and this handoff as work lands. The first increment, recursive filename search, is implemented. Continue from the status below rather than
reimplementing it. Transfer-conflict choices remain a product/design decision because Keep Both is explicit
project behavior; do not silently replace it.

## 5. Other open threads

- Stale paste-cancellation entry in TESTING.md has been removed.
- Default-app resolution, copy metadata fidelity, and cancellation during recursive preparation warrant investigation if selected.
- Session restoration and shared view mode are explicit design choices; do not silently reverse them.

## 6. State right now

Updated by Codex on 12 November 2026; branch `main`, no commits made. The original untracked
`FEATURE-GAPS.md` and `CODEX-HANDOFF.md` are retained and updated. Application changes are in
`src/FolderModel.{h,cpp}`, `src/FileOps.cpp`, `qml/SearchField.qml`, and `qml/Main.qml`; tests in `tests/tst_backend.cpp`
and `tests/shots.cpp`. README, TESTING, and CLAUDE document the behavior.

Implemented:
- Click the search icon to toggle recursive filename search, retaining local filtering by default.
- Background traversal with cancellation through the existing generation token; clearing the query
  or navigating discards obsolete results. Relative result names disambiguate duplicates.
- Hidden subtrees follow Ctrl+H; directory symlinks are not followed. Navigation resets search scope;
  Trash does not offer recursive search. Scope button supports Tab + Space/Enter.
- Result parents are watched so rename/removal refreshes results. New matches in other subfolders
  require F5; unreadable subfolders are skipped. Results land after traversal, with no index/content search.
- Rename validates against the selected result's real parent, including root-level name collisions.
- Compression retains source paths relative to the search root rather than archiving a same-named root file.
  Copy/move/trash/delete/compress ignore selected descendants already covered by a selected real directory;
  selected symlinks remain separate. Regression tests cover contents, both selection orders and symlinks.

Verification completed:
- `cmake --build build`: exit 0.
- `LC_ALL=en_US.UTF-8 QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/tst_backend`:
  64 passed, 0 failed, 0 skipped. New tests cover local vs recursive scope, hidden subtrees, duplicate names,
  symlink loops/broken links, stale queries/navigation, result-parent updates, overlapping selections,
  symlink selections, undo, and nested compression contents.
- `QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/pfm-shots /tmp/pfm-review-shots`:
  all checks passed, including scope click, nested rename, Escape, and the existing workflows.
  Inspected grid.png and search-grid.png against the existing board styling. Logs include invalid-fixture
  thumbnail warnings and one `DelegateModel::cancel: index out range` warning; no check failed.
- `cmake --build build --target all_qmllint`: exit 0, no warnings.
- `git diff --check`: exit 0.
- Independent review's compression and overlapping-operation findings were fixed and regression-tested.
  Broken-symlink concern was disproved by `searchBrokenLink`; no Qt 6.11-only flag was added.

The inherited numeric collation test fails with this environment's `LC_ALL=C.UTF-8`; the full suite passes
with `LC_ALL=en_US.UTF-8`. Do not change application sorting just to hide this environment difference.

No installation, real-drive operations or Hyprland changes were performed. Remaining manual work:
large-tree responsiveness on real storage and desktop keyboard/accessibility behavior. Continue the audit
in focused increments; preserve deliberate choices and avoid treating every competitor feature as required.

## 7. Follow-up: Space quick preview — implemented 12 November 2026

The owner explicitly requested Space on a selected file to show its contents. An optional format-scope
question was unanswered; implemented the common text/image core without additional dependencies.

- `src/Preview.{h,cpp}`: per-popup model, worker-pool read, generation invalidation, read-only regular-file
  validation (nonblocking open + fstat), literal UTF-8/BOM Unicode text capped at 128 KiB with notice,
  image classification and safe local URLs, explicit unsupported/error/empty states.
- `qml/PreviewDialog.qml`: large popup in existing styling, filename and location, scrollable plain text,
  async fit-to-area image (bounded decode dimensions), Space/Escape/Close/outside dismissal, focus return.
  Text that arrives after opening takes scroll focus; pending results are discarded after closing.
- `qml/Main.qml`: Space shortcut only for the focused active file view and exactly one non-folder selection.
  Search/path inputs keep normal spaces; multiple selections do not pick an arbitrary file.
- CMake registers the new source/QML files; README, CLAUDE, TESTING, TODO, KeysDialog and audit updated.
- Plan/checklist: `docs/superpowers/plans/2026-11-12-preview.md`.

Final verification: build exit 0; full backend 67 passed / 0 failed / 0 skipped with
`LC_ALL=en_US.UTF-8 QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/tst_backend`;
`QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/pfm-shots /tmp/pfm-preview-shots` passed
all checks; qmllint exit 0 without warnings; git diff check clean. Inspected preview-text.png,
preview-image.png and keys.png. Logs: `/tmp/pfm-preview-backend.log`, `/tmp/pfm-preview-shots.log`.
The existing invalid-thumbnail and DelegateModel warnings remain in offscreen output.

The pre-implementation UI test failed for missing preview/Space starting search; final tests pass.
Independent review found no important issues; its minor slow-load focus concern was addressed.
Real Wayland/manual checks remain unchecked in TODO.md. PDF, audio/video and office previews are
not implemented. No installation, commit or system configuration changes were made.

## 8. Owner's scope reply: add PDF — implemented 12 November 2026

This supersedes the PDF exclusion in section 7: the owner answered the pending scope question with
text, images and PDF. Poppler `pdfinfo` + `pdftoppm` are installed on this machine; no Qt PDF module
or package installation was needed. README now records Poppler as the PDF runtime requirement.

Preview renders one PDF page at a time, at most 1600 px, and exposes page/count plus guarded
`setPage(int)`. The popup reuses its image area with previous/next controls and Left/Right keys.
Process arguments use QProcess directly (no shell), metadata uses LC_ALL=C, helpers have 10-second
limits and are killed on cancellation with 50-ms polling. Private QTemporaryDir ownership passes
from worker to model and is dropped on page replacement/close; stale callbacks delete their own
results. Metadata retained for page count is capped at 16 KiB. Missing helpers, damaged/encrypted or
slow PDFs show errors. No password prompt or text selection within PDF. Image/PDF zoom was added in the follow-up below.

Added shared real two-page fixture `tests/PreviewPdf.h`; PDF tests cover count, rendered page colours,
size bounds, page boundaries, temporary cleanup, damaged PDFs, missing helpers and active-process
cancellation. Independent review reported no important findings and reproduced the PDF tests.

Final backend: 70 passed, 0 failed, 0 skipped (LC_ALL=en_US.UTF-8); build and qmllint pass. UI checks
cover PDF opening/count, both page buttons, boundaries, keyboard page changes and closing. Inspected
`/tmp/pfm-pdf-shots/preview-pdf-1.png` and `preview-pdf-2.png`: whole pages visible with correct content
and page counter. Logs: `/tmp/pfm-pdf-backend.log`, `/tmp/pfm-pdf-shots.log`.
Manual PDF checks are in TODO.md and TESTING.md. No commit, installation or system-config changes.

## 9. Final follow-up: image/PDF zoom — 12 November 2026

Owner asked only for zoom and then an English commit message to finish the session. Zoom is in
PreviewDialog.qml: a clipped Flickable holds the aspect-fitted image scaled 1–4×, with drag/scroll
panning; −/+ buttons, +/− keys and Ctrl+wheel change scale; Fit/0 resets. setZoom preserves viewport
centre and cancels kinetic motion before adjusting offsets. New files start fitted; PDF page changes
keep zoom. Decoding is async with retained old image, dimensions capped at 4096 px; PDF raster output
remains capped at 1600 px. Icons.qml adds the existing Lucide minus path.

New UI regressions cover controls, painted size, panning, fit/reset, Ctrl+wheel, PDF key zoom and max
clamping. The pre-implementation run failed because zoom controls were absent. Final verification: build exit 0, backend 70 passed / 0 failed / 0 skipped with en_US.UTF-8,
all offscreen UI checks passed, qmllint quiet, git diff check clean. Inspected the zoomed PDF screenshot.
Tests cover Ctrl+wheel, panning, fit reset, zoom bounds and Space closing when Fit has keyboard focus.
Independent review confirmed geometry and focus behavior after the Fit Space override. Screenshots/logs
use /tmp/pfm-zoom-shots, /tmp/pfm-zoom-shots.log and /tmp/pfm-zoom-backend.log. Manual zoom checklist is in TODO.md. No commit or installation requested.

## 10. Default applications settings — implemented 12 November 2026

Owner requested an xdg-mime frontend in Settings and selected scope: popular types plus search of any MIME.
Toolbar sliders button and Ctrl+comma open `qml/SettingsDialog.qml`. The left list starts with common
formats; search covers QMimeDatabase names/MIME IDs and a full custom MIME can be inspected. The right
pane shows the effective xdg-mime default, recommended/all installed app choices, explicit Apply, and a
notice that changes affect the desktop. Current default is first. Keyboard type focus has a neutral
highlight distinct from the inspected MIME's pistachio selection. Selecting an app does not write/launch.

`src/MimeSettings.{h,cpp}` owns async QProcess query/default/verification calls with a 5-second limit per
helper. MIME syntax and installed app IDs are checked; superseded queries cannot update current state.
Saves cannot be superseded by navigation, finish while the popup is closed, and read back the effective
ID before reporting success; higher-priority overrides and tool failures are shown as errors. Depends on
xdg-mime (xdg-utils), no new library. `Apps::forMime` shares existing app ranking/metadata with forFile;
existing Open with launching/default-writing behavior remains unchanged. Its pre-existing resolver
limitations noted in FEATURE-GAPS still apply to Open with; Settings itself uses the real xdg-mime query.

Registration is in CMake; Main/Icons/KeysDialog, README/CLAUDE/TESTING/TODO/audit updated. The toolbar keys
button now has an objectName and shots locate it by identity after the Settings button moved it.
Plan: `docs/superpowers/plans/2026-11-12-default-apps.md`.

Verified: build exit 0; full backend 72 passed / 0 failed / 0 skipped with LC_ALL=en_US.UTF-8; all offscreen
shots checks pass; qmllint quiet; diff check clean. Backend includes a controlled helper for validation,
custom types, no-write lookups, save/readback, stale queries, failed verification and missing/failing tools,
plus the real xdg-mime under scratch HOME preserving unrelated configuration. UI tests cover shortcut,
effective app, no mutation before Apply, verified save, keyboard type navigation, MIME search and close.
Inspected settings-defaults.png, settings-search.png and keys.png under `/tmp/pfm-settings-shots`.
Logs: `/tmp/pfm-settings-backend.log`, `/tmp/pfm-settings-shots.log`. Independent review's keyboard-focus
finding was fixed and re-reviewed; no remaining findings. Existing invalid-thumbnail/DelegateModel
warnings remain in the screenshot runner. User defaults were not modified; real desktop checks remain
unchecked in TODO.md. No commit, installation or system configuration changes performed.
