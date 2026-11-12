# Default applications settings

Owner's scope: popular file types plus lookup of any MIME, in Pistachio FM settings.

Use a toolbar Settings button and Ctrl+comma to open one existing-style popup. A searchable
type list starts with common formats; selecting a type shows the effective xdg-mime default
and installed application choices. Show relevant applications first, with Show all available.
Selecting an application does not write anything; Apply runs xdg-mime default and queries the
result again. Explain desktop-wide scope inside the dialog. No unrelated settings or protocol UI.

Reuse Apps metadata and app ranking through forMime(). Keep the existing Open with behavior.
A per-dialog MimeSettings model owns asynchronous QProcess calls, validates inputs and installed
app IDs, bounds execution, discards superseded queries, and reports failed/overridden saves.
Closing a popup does not interrupt an explicitly requested save. Tests only change scratch HOME.

- [x] Observe missing Settings shortcut in the real UI regression.
- [x] Extract Apps.forMime without changing file-based behavior; add the settings model and tests
  for catalog, lookup, explicit save/readback, invalid inputs, stale queries and helper errors.
- [x] Add Settings popup, toolbar button and shortcut, theme-only styling and keyboard navigation.
- [x] Verify UI default lookup, selection without mutation, Apply, search and dismissal in scratch HOME.
- [x] Build, backend, shots/screenshots, qmllint, diff check and review; update docs and handoff.

Final verification: build, 72 backend checks, complete offscreen UI run, qmllint and diff check passed.
Inspected both Settings states and keys card. Read/write tests use scratch HOME; actual user defaults
remain untouched. Review's keyboard visibility finding was fixed and confirmed by follow-up review.
