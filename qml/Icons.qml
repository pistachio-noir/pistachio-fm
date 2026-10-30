pragma Singleton

import QtQuick

// Lucide path data from the design boards, for Icon.paths. Every path starts with an absolute "M".
QtObject {
    // toolbar
    readonly property list<string> back: ["M12 19l-7-7 7-7", "M19 12H5"]
    readonly property list<string> forward: ["M5 12h14", "M12 5l7 7-7 7"]
    readonly property list<string> up: ["M5 12l7-7 7 7", "M12 19V5"]
    readonly property list<string> chevronRight: ["M9 18l6-6-6-6"]
    readonly property list<string> chevronDown: ["M6 9l6 6 6-6"]
    readonly property list<string> search: ["M3 11a8 8 0 1 0 16 0a8 8 0 1 0-16 0", "M21 21l-4.3-4.3"]
    readonly property list<string> x: ["M18 6 6 18", "M6 6l12 12"]
    readonly property list<string> grid: ["M4 3h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z", "M15 3h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z", "M15 14h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1v-5a1 1 0 0 1 1-1z", "M4 14h5a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1v-5a1 1 0 0 1 1-1z"]
    readonly property list<string> list: ["M8 6h13", "M8 12h13", "M8 18h13", "M3 6h.01", "M3 12h.01", "M3 18h.01"]

    // places and devices
    readonly property list<string> home: ["M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z", "M9 22V12h6v10"]
    readonly property list<string> fileText: ["M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z", "M14 2v4a2 2 0 0 0 2 2h4", "M16 13H8", "M16 17H8"]
    readonly property list<string> download: ["M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4", "M7 10l5 5 5-5", "M12 15V3"]
    readonly property list<string> image: ["M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z", "M7 9a2 2 0 1 0 4 0a2 2 0 1 0-4 0", "M21 15l-3.1-3.1a2 2 0 0 0-2.8 0L6 21"]
    readonly property list<string> music: ["M9 18V5l12-2v13", "M3 18a3 3 0 1 0 6 0a3 3 0 1 0-6 0", "M15 16a3 3 0 1 0 6 0a3 3 0 1 0-6 0"]
    readonly property list<string> drive: ["M22 12H2", "M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z", "M6 16h.01", "M10 16h.01"]
    readonly property list<string> eject: ["M18 9l-6-6-6 6", "M12 3v14", "M5 21h14"] // Lucide's arrow-up-from-line
    readonly property list<string> trash: ["M3 6h18", "M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6", "M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"]

    // file kinds
    readonly property list<string> folder: ["M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"]
    readonly property list<string> file: ["M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z", "M14 2v4a2 2 0 0 0 2 2h4"]
    readonly property list<string> code: ["M16 18l6-6-6-6", "M8 6l-6 6 6 6"]
    readonly property list<string> archive: ["M3 3h18a1 1 0 0 1 1 1v3a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z", "M4 8v11a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8", "M10 12h4"]
    readonly property list<string> film: ["M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z", "M7 3v18", "M3 7.5h4", "M3 12h18", "M3 16.5h4", "M17 3v18", "M17 7.5h4", "M17 16.5h4"]

    // menus and dialogs
    readonly property list<string> pencil: ["M12 20h9", "M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4Z"]
    readonly property list<string> copy: ["M10 8h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H10a2 2 0 0 1-2-2V10a2 2 0 0 1 2-2z", "M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2"]
    readonly property list<string> scissors: ["M3 6a3 3 0 1 0 6 0a3 3 0 1 0-6 0", "M8.12 8.12 12 12", "M20 4 8.12 15.88", "M3 18a3 3 0 1 0 6 0a3 3 0 1 0-6 0", "M14.8 14.8 20 20"]
    readonly property list<string> clipboard: ["M9 2h6a1 1 0 0 1 1 1v2a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z", "M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2"]
    readonly property list<string> link: ["M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71", "M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"]
    readonly property list<string> trash2: ["M3 6h18", "M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6", "M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2", "M10 11v6", "M14 11v6"]
    readonly property list<string> folderPlus: ["M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z", "M12 10v6", "M9 13h6"]
    readonly property list<string> restore: ["M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8", "M3 3v5h5"]
    readonly property list<string> eye: ["M2.062 12.348a1 1 0 0 1 0-.696 10.75 10.75 0 0 1 19.876 0 1 1 0 0 1 0 .696 10.75 10.75 0 0 1-19.876 0", "M9 12a3 3 0 1 0 6 0a3 3 0 1 0-6 0"]
    readonly property list<string> info: ["M2 12a10 10 0 1 0 20 0a10 10 0 1 0-20 0", "M12 16v-4", "M12 8h.01"]
    readonly property list<string> check: ["M20 6 9 17l-5-5"]

    // a kind's glyph (see Format::kind)
    function forKind(kind: string): list<string> {
        switch (kind) {
        case "folder":
            return folder;
        case "document":
            return fileText;
        case "code":
            return code;
        case "image":
            return image;
        case "audio":
            return music;
        case "video":
            return film;
        case "archive":
            return archive;
        default:
            return file;
        }
    }
}
