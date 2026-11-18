# File manager improvements implementation plan

**Goal:** Add the navigation, drag-and-drop, transfer, panel and everyday file-management improvements requested in the comparison.

**Architecture:** Extend the existing Qt models, transfer worker and QML components in small increments. Preserve the current look, desktop integration and data-safety guarantees. Verify backend behavior and user interactions before changing related documentation.

**Tech stack:** C++20, Qt 6, QML, Meson, Clang.

1. Add sort direction and navigation improvements, including tests for selections and tab overflow.
2. Add safe conflict handling and improve transfer cancellation and visibility; test existing files, directories, cancellation and undo.
3. Add batch rename with a preview and validation; test conflicts, cycles and rollback behavior.
4. Add multi-selection properties and permission editing, respecting symlinks and filesystem errors.
5. Improve drag and drop and add requested power-user panels, checking mouse and keyboard interaction.
6. Apply the existing motion tokens to the requested launcher, verify visuals and document completed features.
