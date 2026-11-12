// Runs the real app on a made-up home laid out like the design boards' Documents folder, takes the
// boards' three pictures and a few more, and tries the mouse and keys on it: pfm-shots OUTDIR.
// Writes grid.png, list-menu.png, trash.png, trash-place.png, rename.png, cut.png, paste-menu.png, drag.png, properties.png, open-with.png, new-file.png, tabs-split.png, tab-menu.png, tab-drag.png, keys.png, pictures.png, box-select.png and bookmarks.png,
// and prints a PASS or
// FAIL line per check (exit code 1 on a FAIL). Offscreen (QT_QPA_PLATFORM=offscreen) it doesn't
// touch the desktop; there, an icon of a row cut by the list's edge may show past the edge, which
// on a real display is clipped (checked on Hyprland).

#include "FileOps.h"
#include "FolderModel.h"
#include "Places.h"
#include "MimeSettings.h"
#include "PreviewPdf.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Pistachio_FilesPlugin)

namespace {

int failures = 0;

bool check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    failures += !ok;
    return ok;
}

template<typename Predicate>
bool waitFor(Predicate done, int ms = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms)
        QTest::qWait(20);
    return done();
}

void makeFile(const QString &path, qint64 size, const QDateTime &modified)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.resize(size)) // sparse: no real disk space
        qFatal("can't write %s", qPrintable(path));
    file.setFileTime(modified, QFileDevice::FileModificationTime);
}

void makeFolder(const QString &path, int items)
{
    QDir().mkpath(path);
    for (int i = 0; i < items; ++i)
        makeFile(path + QStringLiteral("/item-%1").arg(i), 0, QDateTime::currentDateTime());
}

// the sidebar's entry for `path`, scrolled into view (real drives plugged in push Trash down): its
// centre in the window, or (-1, -1) if there's none
QPoint placeAt(QQuickWindow *window, const QString &path)
{
    QQuickItem *button = nullptr;
    QList<QQuickItem *> items = { window->contentItem() };
    while (!items.isEmpty() && !button) {
        QQuickItem *item = items.takeFirst();
        if (item->property("ejectable").isValid() && item->property("path").toString() == path)
            button = item;
        items.append(item->childItems());
    }
    if (!button)
        return { -1, -1 };
    for (QQuickItem *flickable = button->parentItem(); flickable; flickable = flickable->parentItem()) {
        if (!flickable->inherits("QQuickFlickable"))
            continue;
        auto *content = flickable->property("contentItem").value<QQuickItem *>();
        const qreal top = button->mapToItem(content, QPointF(0, 0)).y();
        const qreal most = qMax(0.0, flickable->property("contentHeight").toReal() - flickable->height());
        flickable->setProperty("contentY", qBound(0.0, top + button->height() + 8 - flickable->height(), most));
        break;
    }
    QTest::qWait(50);
    return button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
}

// the centre of tab `index` in the tab row, or (-1, -1) if there's none
QPoint tabAt(QQuickWindow *window, int index)
{
    QList<QQuickItem *> items = { window->contentItem() };
    while (!items.isEmpty()) {
        QQuickItem *item = items.takeFirst();
        if (item->property("isCurrent").isValid() && item->property("index").toInt() == index)
            return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        items.append(item->childItems());
    }
    return { -1, -1 };
}

QDrag *findDrag(QQuickItem *item)
{
    if (auto *drag = item->findChild<QDrag *>(Qt::FindDirectChildrenOnly))
        return drag;
    const QList<QQuickItem *> children = item->childItems();
    for (QQuickItem *child : children) {
        if (QDrag *drag = findDrag(child))
            return drag;
    }
    return nullptr;
}

// A left-button drag from `from` to `to`. The mouse starts a real drag, as a person's would; offscreen,
// Qt ends it at once without a drop, so the drop is handed to the window here, carrying the drag's own
// data, as a drop from this window or another app arrives. `modifiers` are held while it drops.
// Returns the URLs the drag carried.
QList<QUrl> dragTo(QQuickWindow *window, QPoint from, QPoint to, Qt::KeyboardModifiers modifiers = {},
                   const QString &snapshot = {})
{
    QTest::mousePress(window, Qt::LeftButton, {}, from);
    QTest::mouseMove(window, from + QPoint(20, 0));
    QTest::mouseMove(window, from + QPoint(40, 0));
    QDrag *drag = findDrag(window->contentItem()); // still there until the next qWait deletes it
    QMimeData data;
    if (drag) {
        for (const QString &format : drag->mimeData()->formats())
            data.setData(format, drag->mimeData()->data(format));
    }
    QTest::mouseRelease(window, Qt::LeftButton, {}, from + QPoint(40, 0));
    if (!drag)
        return {};

    if (modifiers & Qt::ControlModifier)
        QTest::keyPress(window, Qt::Key_Control, Qt::ControlModifier);
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent enter(QPointF(to), actions, &data, Qt::LeftButton, modifiers);
    QCoreApplication::sendEvent(window, &enter);
    QDragMoveEvent move(QPointF(to), actions, &data, Qt::LeftButton, modifiers);
    QCoreApplication::sendEvent(window, &move);
    QTest::qWait(250);
    if (!snapshot.isEmpty())
        window->grabWindow().save(snapshot);
    QDropEvent drop(QPointF(to), actions, &data, Qt::NoButton, modifiers);
    QCoreApplication::sendEvent(window, &drop);
    if (modifiers & Qt::ControlModifier)
        QTest::keyRelease(window, Qt::Key_Control);
    QTest::qWait(100);
    return data.urls();
}

void save(QQuickWindow *window, const QString &path)
{
    QTest::qWait(350); // popups' fades, the counts landing
    const QImage image = window->grabWindow();
    check(!image.isNull() && image.save(path), qPrintable(QStringLiteral("saved ") + path));
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: pfm-shots OUTDIR\n");
        return 2;
    }
    const QString out = QDir(QString::fromLocal8Bit(argv[1])).absolutePath();
    QDir().mkpath(out);

    // fonts first, from the real home (~/.local/share/fonts), before HOME moves to the made-up one
    QGuiApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("1 · a1b2c3d")); // a sample for the status bar's corner (the app's comes from git)
    app.setDesktopFileName(QStringLiteral("pistachio-fm-shots"));
    QFontDatabase::families();

    QTemporaryDir root;
    const QString home = root.path() + QStringLiteral("/home");
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.local/share")).toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());

    // the boards' Documents (names, sizes and dates as drawn there)
    const QString docs = home + QStringLiteral("/Documents");
    for (const char *place : { "Desktop", "Documents", "Downloads", "Pictures", "Music" })
        QDir().mkpath(home + u'/' + QLatin1String(place));
    makeFolder(docs + QStringLiteral("/Projects"), 14);
    makeFolder(docs + QStringLiteral("/Notes"), 9);
    makeFolder(docs + QStringLiteral("/dotfiles"), 31);
    makeFolder(docs + QStringLiteral("/Wallpapers"), 42);
    const int year = QDate::currentDate().year();
    const auto october = [year](int day) { return QDateTime(QDate(year, 10, day), QTime(12, 0)); };
    makeFile(docs + QStringLiteral("/thesis-draft.pdf"), 2400000, october(27));
    makeFile(docs + QStringLiteral("/todo.md"), 2048, QDateTime::currentDateTime());
    makeFile(docs + QStringLiteral("/init.lua"), 4096, october(20));
    makeFile(docs + QStringLiteral("/wallpaper-04.png"), 3100000, october(22));
    makeFile(docs + QStringLiteral("/mixtape.flac"), 34000000, october(19));
    makeFile(docs + QStringLiteral("/boards-export.zip"), 18000000, october(25));
    makeFile(docs + QStringLiteral("/receipt-oct.pdf"), 88000, october(12));

    // a stand-in app for Markdown, so Open with never starts a real one: it notes the file it was given
    const QString opened = home + QStringLiteral("/opened.txt");
    const QString fakeApp = home + QStringLiteral("/fake-editor.sh");
    {
        QFile script(fakeApp);
        if (!script.open(QIODevice::WriteOnly)
            || script.write("#!/bin/sh\nprintf '%s' \"$1\" > '" + opened.toUtf8() + "'\n") < 0)
            qFatal("can't write the stand-in app");
        script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QDir().mkpath(home + QStringLiteral("/.local/share/applications"));
        QFile entry(home + QStringLiteral("/.local/share/applications/pfm-fake.desktop"));
        if (!entry.open(QIODevice::WriteOnly)
            || entry.write("[Desktop Entry]\nType=Application\nName=Fake Editor\nExec=" + fakeApp.toUtf8()
                           + " %f\nMimeType=text/markdown;\n") < 0)
            qFatal("can't write the stand-in app's entry");
    }

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("startPath"), docs } });
    engine.loadFromModule("Pistachio.Files", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) {
        std::fprintf(stderr, "the window didn't load\n");
        return 1;
    }
    auto *folder = qobject_cast<FolderModel *>(window->property("folder").value<QObject *>());
    if (!folder) {
        std::fprintf(stderr, "no folder model on the window\n");
        return 1;
    }

    check(waitFor([&] { return window->isExposed() && !folder->loading() && folder->count() == 11; }),
          "Documents lists its 11 entries");
    check(waitFor([&] {
              return folder->data(folder->index(0), FolderModel::DetailRole).toString() == QLatin1String("31 items");
          }),
          "dotfiles counts 31 items");

    // 1. the grid, as on browse-grid.html
    save(window, out + QStringLiteral("/grid.png"));

    // the mouse on the grid (four columns of 174.5 px tiles from x 212, rows of 108 from y 76)
    const QPoint firstTile(299, 130);
    const QPoint secondTile(484, 130);
    const QPoint empty(600, 448);
    QTest::mouseClick(window, Qt::LeftButton, {}, firstTile);
    check(folder->selectedCount() == 1 && folder->currentRow() == 0, "a click selects a tile");
    QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier, secondTile);
    check(folder->selectedCount() == 2, "Ctrl+click adds a tile");
    QTest::mouseClick(window, Qt::LeftButton, {}, empty);
    check(folder->selectedCount() == 0, "a click on empty space clears the selection");
    QTest::keyClick(window, Qt::Key_Right);
    QTest::keyClick(window, Qt::Key_Right, Qt::ShiftModifier);
    check(folder->selectedCount() == 2 && folder->currentRow() == 1, "arrow keys start over after a clear; Shift extends");
    QTest::mouseDClick(window, Qt::LeftButton, {}, firstTile);
    check(waitFor([&] { return folder->path() == docs + QStringLiteral("/dotfiles"); }), "a double-click opens a folder");
    QTest::keyClick(window, Qt::Key_Backspace);
    check(waitFor([&] { return folder->path() == docs && !folder->loading(); }), "Backspace goes up");
    check(folder->selectedPaths() == QStringList { docs + QStringLiteral("/dotfiles") }, "going up selects where you were");
    for (const char key : { 'p', 'd', 'f' })
        QTest::keyClick(window, key);
    check(folder->filter() == QLatin1String("pdf") && folder->count() == 2, "typing over the files searches");
    QTest::keyClick(window, Qt::Key_Escape);
    check(folder->filter().isEmpty() && folder->count() == 11, "Escape clears the search");

    // The same search field can traverse subfolders; result actions must use the actual parent.
    const QString searchFile = docs + QStringLiteral("/Notes/search-note.md");
    makeFile(searchFile, 42, QDateTime::currentDateTime());
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    for (const char key : QByteArray("search-note"))
        QTest::keyClick(window, key);
    check(folder->count() == 0, "the normal filter does not search subfolders");
    auto *scope = window->findChild<QQuickItem *>(QStringLiteral("searchScope"));
    if (check(scope != nullptr, "search has a scope toggle")) {
        QTest::mouseClick(window, Qt::LeftButton, {},
                          scope->mapToScene(QPointF(22, 22)).toPoint());
        check(waitFor([&] { return !folder->loading() && folder->rowOf(searchFile) >= 0; }),
              "the search icon enables recursive filename search");
        save(window, out + QStringLiteral("/search-grid.png"));
        QTest::keyClick(window, Qt::Key_Return);
        QTest::keyClick(window, Qt::Key_F2);
        QTest::qWait(250);
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const char key : QByteArray("todo.md"))
            QTest::keyClick(window, key);
        QTest::keyClick(window, Qt::Key_Return);
        check(waitFor([&] { return QFile::exists(docs + QStringLiteral("/Notes/todo.md")); }),
              "renaming a nested result checks its own folder, not the search root");
        QTest::qWait(300);
        QFile::remove(docs + QStringLiteral("/Notes/todo.md"));
        QTest::keyClick(window, Qt::Key_Escape);
        check(waitFor([&] { return !folder->loading() && folder->count() == 11; }),
              "Escape leaves recursive results and restores the root listing");
        folder->setRecursive(false);
        waitFor([&] { return !folder->loading(); });
    }
    QFile::remove(searchFile);

    // 2. the list with the menu, as on list-context-menu.html
    folder->selectPaths({ docs + QStringLiteral("/thesis-draft.pdf") });
    QMetaObject::invokeMethod(window, "setView", Q_ARG(QString, QStringLiteral("list")));
    QTest::qWait(100);
    QMetaObject::invokeMethod(window, "openMenuAt", Q_ARG(double, 520), Q_ARG(double, 224));
    QTest::qWait(250);
    QTest::mouseMove(window, QPoint(620, 224 + 8 + 44 + 22)); // over Rename, the board's hovered entry
    save(window, out + QStringLiteral("/list-menu.png"));
    QTest::keyClick(window, Qt::Key_Escape);

    // 3. the trash dialog, as on trash-dialog.html
    QTest::qWait(250);
    folder->selectPaths({ docs + QStringLiteral("/thesis-draft.pdf"), docs + QStringLiteral("/boards-export.zip") });
    QMetaObject::invokeMethod(window, "askTrash");
    save(window, out + QStringLiteral("/trash.png"));
    QTest::keyClick(window, Qt::Key_Return); // focus starts on "Move to trash"
    check(waitFor([&] { return folder->count() == 9; }), "Move to trash empties the two from the folder");
    check(QFile::exists(home + QStringLiteral("/.local/share/Trash/files/thesis-draft.pdf")), "…into the trash");

    // 4. the Trash place: its menu restores
    QTest::qWait(250);
    auto *places = engine.singletonInstance<Places *>("Pistachio.Files", "Places");
    QTest::mouseClick(window, Qt::LeftButton, {}, placeAt(window, places->trash()));
    if (!check(waitFor([&] { return folder->path().endsWith(QLatin1String("/Trash/files")) && folder->count() == 2; }),
               "the sidebar opens the Trash")) {
        std::printf("stopped: the next steps would click around in %s\n", qPrintable(folder->path()));
        return 1;
    }
    QMetaObject::invokeMethod(window, "setView", Q_ARG(QString, QStringLiteral("grid")));
    QTest::qWait(100);
    auto *emptyChip = window->findChild<QQuickItem *>(QStringLiteral("emptyTrash"));
    check(emptyChip && emptyChip->isVisible(), "in the Trash, the status bar offers Empty trash");
    QObject *confirm = window->findChild<QObject *>(QStringLiteral("confirm"));
    if (emptyChip && confirm) {
        QTest::mouseClick(window, Qt::LeftButton, {},
                          emptyChip->mapToScene(QPointF(emptyChip->width() / 2, emptyChip->height() / 2)).toPoint());
        check(waitFor([&] { return confirm->property("opened").toBool(); }), "…which asks first");
        QTest::keyClick(window, Qt::Key_Escape);
        check(waitFor([&] { return !confirm->property("visible").toBool(); }) && folder->count() == 2,
              "…and Cancel keeps everything");
        QTest::qWait(300);
    }
    QTest::mouseClick(window, Qt::RightButton, {}, firstTile);
    save(window, out + QStringLiteral("/trash-place.png"));
    QTest::mouseClick(window, Qt::LeftButton, {}, firstTile + QPoint(60, 8 + 22)); // the first entry
    check(waitFor([&] { return QFile::exists(docs + QStringLiteral("/boards-export.zip")); }), "Restore puts it back");

    // 5. renaming, with F2. Qt holds the window's shortcuts while a popup that Escape closes is on
    // screen, its 120 ms fade-out included, so first let the menu go.
    QTest::qWait(300);
    QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
    check(window->property("viewMode").toString() == QLatin1String("list"), "Ctrl+2 shows the list");
    QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
    check(waitFor([&] { return folder->path() == docs && folder->count() == 10; }), "Alt+Left goes back");
    folder->selectPaths({ docs + QStringLiteral("/todo.md") });
    QTest::keyClick(window, Qt::Key_F2);
    QTest::qWait(250);
    for (const char key : { 'd', 'o', 'n', 'e' })
        QTest::keyClick(window, key); // over the selected "todo"
    save(window, out + QStringLiteral("/rename.png"));
    QTest::keyClick(window, Qt::Key_Return);
    check(waitFor([&] { return folder->selectedPaths() == QStringList { docs + QStringLiteral("/done.md") }; }),
          "F2 renames, and the new name is selected");

    // 6. copy, cut, paste and undo, with the keys and the menu
    QTest::qWait(300);
    auto *ops = engine.singletonInstance<FileOps *>("Pistachio.Files", "FileOps");
    const QString notes = docs + QStringLiteral("/Notes");
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    check(waitFor([&] { return folder->selectedPaths() == QStringList { docs + QStringLiteral("/todo.md") }; }),
          "Ctrl+Z takes the rename back");
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    check(ops->canPaste() && ops->cutPaths().isEmpty(), "Ctrl+C copies the selection");
    QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
    folder->go(notes);
    waitFor([&] { return folder->path() == notes && !folder->loading(); });
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    check(waitFor([&] {
              return folder->count() == 10 && folder->selectedPaths() == QStringList { notes + QStringLiteral("/todo.md") };
          }),
          "Ctrl+V pastes it into another folder, selected");
    check(QFile::exists(docs + QStringLiteral("/todo.md")), "…and the original stays");
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    check(waitFor([&] { return folder->count() == 9; }), "Ctrl+Z takes the copy back");

    // cut in Documents, pasted in Notes from the menu on empty space
    QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
    waitFor([&] { return folder->path() == docs && !folder->loading(); });
    folder->selectPaths({ docs + QStringLiteral("/init.lua") });
    QTest::keyClick(window, Qt::Key_X, Qt::ControlModifier);
    check(ops->cutPaths() == QStringList { docs + QStringLiteral("/init.lua") }, "Ctrl+X cuts the selection");
    folder->clearSelection();
    save(window, out + QStringLiteral("/cut.png")); // init.lua faded
    folder->go(notes);
    waitFor([&] { return folder->path() == notes && !folder->loading(); });
    QTest::mouseClick(window, Qt::RightButton, {}, QPoint(600, 520));
    save(window, out + QStringLiteral("/paste-menu.png"));
    for (const auto key : { Qt::Key_Down, Qt::Key_Down, Qt::Key_Down, Qt::Key_Return }) // New folder, New file, Paste
        QTest::keyClick(window, key);
    check(waitFor([&] { return QFile::exists(notes + QStringLiteral("/init.lua")); })
              && !QFile::exists(docs + QStringLiteral("/init.lua")),
          "the menu's Paste moves what was cut");
    check(!ops->canPaste() && ops->cutPaths().isEmpty(), "…and the clipboard lets go of it");

    // a text field keeps Ctrl+V for its own text
    QTest::qWait(300);
    ops->copyPaths({ QStringLiteral("lua") });
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    check(waitFor([&] { return folder->filter() == QLatin1String("lua") && folder->count() == 1; }),
          "Ctrl+V in the search field pastes text there");

    // 7. drag and drop, back in Documents' grid: dotfiles, Notes, Projects, Wallpapers, boards-export.zip,
    // mixtape.flac, receipt-oct.pdf, todo.md, wallpaper-04.png
    QTest::keyClick(window, Qt::Key_Escape); // the search
    QTest::qWait(100);
    folder->go(docs);
    waitFor([&] { return folder->path() == docs && !folder->loading() && folder->count() == 9; });
    QTest::qWait(300);
    const auto tile = [](int index) { return QPoint(299 + 185 * (index % 4), 130 + 118 * (index / 4)); };
    const QString downloads = home + QStringLiteral("/Downloads");
    QTest::mouseClick(window, Qt::LeftButton, {}, tile(7));
    check(dragTo(window, tile(7), tile(1), {}, out + QStringLiteral("/drag.png"))
              == QList<QUrl> { QUrl::fromLocalFile(docs + QStringLiteral("/todo.md")) },
          "a drag carries the selection's file URLs");
    check(waitFor([&] { return QFile::exists(notes + QStringLiteral("/todo.md")); })
              && !QFile::exists(docs + QStringLiteral("/todo.md")),
          "dragging a file onto a folder moves it in");
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    check(waitFor([&] { return QFile::exists(docs + QStringLiteral("/todo.md")); }), "Ctrl+Z moves it back");

    QTest::qWait(300);
    QTest::mouseClick(window, Qt::LeftButton, {}, tile(5));
    QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier, tile(6));
    check(dragTo(window, tile(5), placeAt(window, downloads), Qt::ControlModifier).size() == 2,
          "pressing one of two selected files drags both");
    check(waitFor([&] {
              return QFile::exists(downloads + QStringLiteral("/mixtape.flac")) && QFile::exists(downloads + QStringLiteral("/receipt-oct.pdf"));
          }) && QFile::exists(docs + QStringLiteral("/mixtape.flac")),
          "two selected files dragged onto a place, with Ctrl, are copied there");
    QTest::mouseClick(window, Qt::LeftButton, {}, tile(5));
    check(folder->selectedPaths() == QStringList { docs + QStringLiteral("/mixtape.flac") },
          "a click on one of several selected picks just it");

    QTest::qWait(300);
    dragTo(window, tile(8), placeAt(window, places->trash()));
    check(waitFor([&] { return QFile::exists(home + QStringLiteral("/.local/share/Trash/files/wallpaper-04.png")); }),
          "dragging onto Trash moves it to the trash");
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    check(waitFor([&] { return QFile::exists(docs + QStringLiteral("/wallpaper-04.png")); }), "…and Ctrl+Z brings it back");

    // 8. properties, with Alt+Enter: a folder's, whose size fills in once counted
    QTest::qWait(300);
    QTest::mouseClick(window, Qt::LeftButton, {}, tile(2)); // Projects
    QTest::keyClick(window, Qt::Key_Return, Qt::AltModifier);
    QObject *properties = window->findChild<QObject *>(QStringLiteral("properties"));
    check(properties && waitFor([&] { return properties->property("size").toString() == QStringLiteral("0 B · 14 files"); }),
          "Alt+Enter shows a folder's properties, its size counted");
    save(window, out + QStringLiteral("/properties.png"));
    QTest::keyClick(window, Qt::Key_Escape);
    check(properties && waitFor([&] { return !properties->property("visible").toBool(); }), "Escape closes them");

    // 9. Open with, from todo.md's menu: the stand-in app picked, with Always use on
    QTest::qWait(300);
    QTest::mouseMove(window, QPoint(100, 560)); // off where the menu opens
    folder->selectPaths({ docs + QStringLiteral("/todo.md") });
    QMetaObject::invokeMethod(window, "openMenuAt", Q_ARG(double, 520), Q_ARG(double, 224));
    QTest::qWait(250);
    for (const auto key : { Qt::Key_Down, Qt::Key_Down, Qt::Key_Return }) // Open, Open with…
        QTest::keyClick(window, key);
    QObject *openWith = window->findChild<QObject *>(QStringLiteral("openWith"));
    check(openWith && waitFor([&] { return openWith->property("opened").toBool(); }), "the menu's Open with… opens the card");
    if (openWith) {
        bool listed = false;
        for (const QVariant &app : openWith->property("choices").toList())
            listed |= app.toMap().value(QStringLiteral("id")) == QLatin1String("pfm-fake.desktop");
        check(listed, "…listing the apps for Markdown");
        openWith->setProperty("chosen", QStringLiteral("pfm-fake.desktop"));
        openWith->setProperty("remember", true);
        save(window, out + QStringLiteral("/open-with.png"));
        QTest::keyClick(window, Qt::Key_Return);
        check(waitFor([&] {
                  QFile log(opened);
                  return log.open(QIODevice::ReadOnly) && log.readAll() == (docs + QStringLiteral("/todo.md")).toUtf8();
              }),
              "Enter opens the file with the picked app");
        QFile defaults(home + QStringLiteral("/.config/mimeapps.list"));
        check(defaults.open(QIODevice::ReadOnly) && defaults.readAll().contains("text/markdown=pfm-fake.desktop"),
              "…and Always use makes it the default for Markdown");
        // from now on Enter on the file opens it with that app, nothing asked in between
        QTest::qWait(300);
        QFile::remove(opened);
        QTest::keyClick(window, Qt::Key_Return);
        check(waitFor([&] {
                  QFile log(opened);
                  return log.open(QIODevice::ReadOnly) && log.readAll() == (docs + QStringLiteral("/todo.md")).toUtf8();
              }),
              "Enter on the file then opens it with the default");
        QTest::keyClick(window, Qt::Key_Return, Qt::AltModifier);
        check(properties && waitFor([&] { return properties->property("opensWith").toString() == QLatin1String("Fake Editor"); }),
              "Properties says what opens it now");
        QTest::keyClick(window, Qt::Key_Escape);
    }

    // 10. New file, from the menu on empty space: the name typed over "New file", the extension changed
    QTest::qWait(300);
    folder->clearSelection();
    QMetaObject::invokeMethod(window, "openMenuAt", Q_ARG(double, 520), Q_ARG(double, 224));
    QTest::qWait(250);
    for (const auto key : { Qt::Key_Down, Qt::Key_Down, Qt::Key_Return }) // New folder, New file
        QTest::keyClick(window, key);
    QTest::qWait(300);
    for (const char key : { 'n', 'o', 't', 'e', 's' })
        QTest::keyClick(window, key); // over the selected "New file"
    QTest::keyClick(window, Qt::Key_End);
    for (int i = 0; i < 3; ++i)
        QTest::keyClick(window, Qt::Key_Backspace); // "txt"
    for (const char key : { 'm', 'd' })
        QTest::keyClick(window, key);
    save(window, out + QStringLiteral("/new-file.png"));
    QTest::keyClick(window, Qt::Key_Return);
    check(waitFor([&] { return folder->selectedPaths() == QStringList { docs + QStringLiteral("/notes.md") }; })
              && QFileInfo(docs + QStringLiteral("/notes.md")).size() == 0,
          "New file makes an empty notes.md, selected");

    // 11. Desktop, one of the places
    QTest::qWait(300);
    QTest::mouseClick(window, Qt::LeftButton, {}, placeAt(window, home + QStringLiteral("/Desktop")));
    check(waitFor([&] { return folder->path() == home + QStringLiteral("/Desktop"); }), "Desktop is one of the places");
    save(window, out + QStringLiteral("/desktop-place.png"));

    // 12. tabs and split view, back in Documents: each tab and pane keeps its own folder
    QTest::qWait(300);
    folder->go(docs);
    waitFor([&] { return folder->path() == docs && !folder->loading(); });
    const auto current = [&] { return window->property("folder").value<QObject *>()->property("path").toString(); };
    const auto page = [&] { return window->property("page").value<QObject *>(); };
    const auto tabCount = [&] { return window->property("tabTitles").toList().size(); };
    QTest::keyClick(window, Qt::Key_T, Qt::ControlModifier);
    check(waitFor([&] { return tabCount() == 2 && current() == docs; }), "Ctrl+T opens a second tab on the same folder");
    QMetaObject::invokeMethod(window->property("folder").value<QObject *>(), "go", Q_ARG(QString, notes));
    waitFor([&] { return current() == notes; });
    QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
    check(waitFor([&] { return current() == docs; }), "Ctrl+Tab goes back to the first, still in Documents");
    QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
    check(waitFor([&] { return current() == notes; }), "…and the second is still in Notes");

    QTest::keyClick(window, Qt::Key_F3);
    check(waitFor([&] { return page()->property("split").toBool() && page()->property("currentPane").toInt() == 1 && current() == notes; }),
          "F3 splits the tab, the new pane on the same folder");
    QMetaObject::invokeMethod(window->property("folder").value<QObject *>(), "go", Q_ARG(QString, docs));
    check(waitFor([&] { return page()->property("title").toString() == QStringLiteral("Notes · Documents"); }),
          "each pane goes its own way, and the tab says both");
    save(window, out + QStringLiteral("/tabs-split.png"));
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(300, 210)); // a tile in the first pane
    check(waitFor([&] { return page()->property("currentPane").toInt() == 0 && current() == notes; }),
          "a click in the other pane makes it the one the toolbar follows");
    QTest::keyClick(window, Qt::Key_F3);
    check(waitFor([&] { return !page()->property("split").toBool() && current() == notes; }), "F3 again closes the split, keeping that pane");
    QTest::keyClick(window, Qt::Key_W, Qt::ControlModifier);
    check(waitFor([&] { return tabCount() == 1 && current() == docs; }), "Ctrl+W closes the tab; one left, the row goes");

    QTest::qWait(300);
    QTest::mouseClick(window, Qt::MiddleButton, {}, tile(1)); // Notes
    check(waitFor([&] { return tabCount() == 2 && current() == notes; }), "middle-click on a folder opens it in a new tab");

    // a tab's right-click menu, then a tab dragged onto a side of the files: Documents (split) onto Notes' left
    const auto currentTab = [&] { return window->property("currentTab").toInt(); };
    QTest::qWait(300);
    QTest::mouseClick(window, Qt::RightButton, {}, tabAt(window, 0));
    QTest::qWait(300);
    save(window, out + QStringLiteral("/tab-menu.png"));
    for (const auto key : { Qt::Key_Down, Qt::Key_Return }) // Split view
        QTest::keyClick(window, key);
    QTest::qWait(400); // past the menu's fade, which hands the keyboard back
    check(currentTab() == 0 && page()->property("split").toBool() && page()->property("currentPane").toInt() == 1,
          "right-click a tab: Split view goes to it and splits it, the new pane the one you're in");
    QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
    waitFor([&] { return currentTab() == 1; });
    const QPoint from = tabAt(window, 0);
    QTest::mousePress(window, Qt::LeftButton, {}, from);
    for (const QPoint to : { from + QPoint(0, 20), QPoint(300, 220), QPoint(300, 330) }) // the files' left third
        QTest::mouseMove(window, to);
    check(window->property("dropSide").toString() == QStringLiteral("left") && window->property("draggedTab").toInt() == 0,
          "a tab dragged over the files' left lights that side, its chip under the pointer");
    save(window, out + QStringLiteral("/tab-drag.png"));
    QTest::mouseRelease(window, Qt::LeftButton, {}, QPoint(300, 330));
    check(waitFor([&] { return tabCount() == 1 && page()->property("title").toString() == QStringLiteral("Documents · Notes"); }),
          "…and dropped there, its folder is the left pane and the tab goes");
    QTest::qWait(300);
    check(page()->property("currentPane").toInt() == 0 && current() == docs && window->property("dropSide").toString().isEmpty()
              && window->property("draggedTab").toInt() == -1,
          "…the dropped pane the one you're in");
    QTest::keyClick(window, Qt::Key_T, Qt::ControlModifier);
    waitFor([&] { return tabCount() == 2; });
    QTest::mouseClick(window, Qt::RightButton, {}, tabAt(window, 1));
    QTest::qWait(300);
    for (const auto key : { Qt::Key_Down, Qt::Key_Down, Qt::Key_Return }) // Close tab
        QTest::keyClick(window, key);
    check(waitFor([&] { return tabCount() == 1; }), "Close tab in a tab's menu closes it");

    // 13. the toolbar's keyboard button lists every key
    QTest::qWait(300);
    auto *keysButton = window->findChild<QQuickItem *>(QStringLiteral("keysButton"));
    if (check(keysButton != nullptr, "the toolbar has a keyboard shortcuts button"))
        QTest::mouseClick(window, Qt::LeftButton, {}, keysButton->mapToScene(QPointF(22, 22)).toPoint());
    QObject *keysCard = window->findChild<QObject *>(QStringLiteral("keys"));
    check(keysCard && waitFor([&] { return keysCard->property("opened").toBool(); }), "the keyboard button lists the keys");
    save(window, out + QStringLiteral("/keys.png"));
    QTest::keyClick(window, Qt::Key_Escape);
    check(keysCard && waitFor([&] { return !keysCard->property("visible").toBool(); }), "…and Escape closes it");

    // 14. a picture shows itself in its badge; anything else keeps the kind's glyph
    QTest::qWait(300);
    const QString pictures = home + QStringLiteral("/Pictures");
    QDir().mkpath(pictures);
    QImage sunset(400, 300, QImage::Format_RGB32);
    for (int y = 0; y < sunset.height(); ++y)
        std::fill_n(reinterpret_cast<QRgb *>(sunset.scanLine(y)), sunset.width(), qRgb(230, 120 + y / 3, 60));
    sunset.save(pictures + QStringLiteral("/sunset #1.png")); // a name that needs escaping in a URL
    makeFile(pictures + QStringLiteral("/broken.png"), 2000, QDateTime::currentDateTime());
    // the first tab's folder went with it (the drag above): from here on, the one on show
    folder = qobject_cast<FolderModel *>(window->property("folder").value<QObject *>());
    folder->go(pictures);
    QQuickItem *thumbnail = nullptr;
    const auto thumbnails = [&] {
        int ready = 0;
        QList<QQuickItem *> items = { window->contentItem() };
        while (!items.isEmpty()) {
            QQuickItem *item = items.takeFirst();
            if (item->inherits("QQuickImage") && item->property("status").toInt() == 1 /* Ready */
                && item->property("source").toUrl().toString().startsWith(QStringLiteral("image://thumbnail/"))) {
                ++ready;
                thumbnail = item;
            }
            items.append(item->childItems());
        }
        return ready;
    };
    check(waitFor([&] { return folder->path() == pictures && thumbnails() == 1; }), "a picture shows itself in its badge, a broken one doesn't");
    QTest::qWait(100);
    if (thumbnail) {
        const QPoint middle = thumbnail->mapToScene(QPointF(thumbnail->width() / 2, thumbnail->height() / 2)).toPoint();
        const QColor seen = window->grabWindow().pixelColor(middle * window->devicePixelRatio());
        check(seen.red() > 200 && seen.blue() < 100, qPrintable(QStringLiteral("…the sunset's orange, there (%1)").arg(seen.name())));
    }
    save(window, out + QStringLiteral("/pictures.png"));

    // 15. a box dragged from empty space selects what it touches; Ctrl keeps what was selected
    QTest::qWait(300);
    QTest::mousePress(window, Qt::LeftButton, {}, QPoint(480, 320)); // under the two pictures
    for (const QPoint to : { QPoint(470, 300), QPoint(400, 220), QPoint(300, 150) })
        QTest::mouseMove(window, to);
    check(waitFor([&] { return folder->selectedCount() == 2; }), "a box dragged over both pictures selects both");
    save(window, out + QStringLiteral("/box-select.png"));
    QTest::mouseRelease(window, Qt::LeftButton, {}, QPoint(300, 150));
    QTest::qWait(100);
    check(folder->selectedCount() == 2, "…and they stay selected after");
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(480, 320));
    check(waitFor([&] { return folder->selectedCount() == 0; }), "a click on empty space clears them");
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(300, 150)); // broken.png
    QTest::mousePress(window, Qt::LeftButton, Qt::ControlModifier, QPoint(540, 320));
    for (const QPoint to : { QPoint(530, 300), QPoint(500, 150) }) // only over the sunset
        QTest::mouseMove(window, to);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::ControlModifier, QPoint(500, 150));
    check(waitFor([&] { return folder->selectedCount() == 2; }), "with Ctrl, the box adds to what was selected");
    QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier); // the list
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(480, 400));
    QTest::mousePress(window, Qt::LeftButton, {}, QPoint(480, 400));
    for (const QPoint to : { QPoint(470, 380), QPoint(400, 300), QPoint(400, 200) }) // up to the second row
        QTest::mouseMove(window, to);
    QTest::mouseRelease(window, Qt::LeftButton, {}, QPoint(400, 200));
    check(waitFor([&] { return folder->selectedCount() == 1; }), "in the list, the box selects the rows it touches");
    QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);

    // 16. Ctrl+D bookmarks the folder you're in: it's under Bookmarks, lit, and opens from there; right-click removes it
    QTest::qWait(300);
    folder->go(notes);
    waitFor([&] { return folder->path() == notes; });
    QTest::keyClick(window, Qt::Key_D, Qt::ControlModifier);
    check(waitFor([&] { return places->isBookmarked(notes) && placeAt(window, notes).x() >= 0; }),
          "Ctrl+D bookmarks the folder, and the sidebar lists it");
    save(window, out + QStringLiteral("/bookmarks.png"));
    folder->go(home);
    waitFor([&] { return folder->path() == home; });
    QTest::mouseClick(window, Qt::LeftButton, {}, placeAt(window, notes));
    check(waitFor([&] { return folder->path() == notes; }), "…and opens from there");
    QTest::mouseClick(window, Qt::RightButton, {}, placeAt(window, notes));
    QTest::qWait(300);
    for (const auto key : { Qt::Key_Down, Qt::Key_Return }) // Remove from bookmarks
        QTest::keyClick(window, key);
    check(waitFor([&] { return !places->isBookmarked(notes) && placeAt(window, notes).x() < 0; }), "right-click, Remove takes it off");

    // Space previews the selected file rather than appending a space to the search.
    QTest::qWait(250); // the preceding bookmark menu must finish releasing the shortcuts
    folder->go(docs);
    waitFor([&] { return !folder->loading(); });
    const QString previewPath = docs + QStringLiteral("/preview.txt");
    {
        QFile file(previewPath);
        if (file.open(QIODevice::WriteOnly))
            file.write("A plain text preview.\n<em>This stays literal.</em>\n");
        QFile unsupported(docs + QStringLiteral("/preview.docx"));
        if (unsupported.open(QIODevice::WriteOnly))
            unsupported.write("%PDF-1.4\n");
    }
    folder->reload();
    waitFor([&] { return !folder->loading() && folder->rowOf(previewPath) >= 0; });
    folder->selectPaths({ previewPath });
    QMetaObject::invokeMethod(window, "focusFiles");
    QTest::keyClick(window, Qt::Key_Space);
    QObject *preview = window->findChild<QObject *>(QStringLiteral("preview"));
    check(preview && waitFor([&] { return preview->property("opened").toBool(); }),
          "Space opens a preview of the selected file");
    check(folder->filter().isEmpty(), "Space over files does not start a search");
    if (preview) {
        check(waitFor([&] {
            return preview->property("content").toMap().value(QStringLiteral("text")).toString()
                == QLatin1String("A plain text preview.\n<em>This stays literal.</em>\n");
        }), "the preview reads the selected file's actual text");
        save(window, out + QStringLiteral("/preview-text.png"));
        QTest::keyClick(window, Qt::Key_Space);
        check(waitFor([&] { return !preview->property("visible").toBool(); }), "Space closes the preview");
        QTest::qWait(250);
        QTest::keyClick(window, Qt::Key_Space);
        waitFor([&] { return preview->property("opened").toBool(); });
        QTest::keyClick(window, Qt::Key_Escape);
        check(waitFor([&] { return !preview->property("visible").toBool(); }), "Escape closes the preview");
        QTest::qWait(250);
        QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
        for (const char key : QByteArray("two words"))
            QTest::keyClick(window, key);
        check(folder->filter() == QLatin1String("two words") && !preview->property("visible").toBool(),
              "Space in the search field remains normal text input");
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Space);
        check(!preview->property("visible").toBool(), "Space in the path field does not open preview");
        QTest::keyClick(window, Qt::Key_Escape);
        folder->clearSelection();
        QMetaObject::invokeMethod(window, "focusFiles");
        QTest::keyClick(window, Qt::Key_Space);
        check(!preview->property("visible").toBool() && folder->filter().isEmpty(),
              "Space with no selection leaves browsing unchanged");
        folder->selectPaths({ previewPath, docs + QStringLiteral("/todo.md") });
        QTest::keyClick(window, Qt::Key_Space);
        check(!preview->property("visible").toBool(), "Space with multiple files does not pick an arbitrary preview");

        folder->go(pictures);
        waitFor([&] { return !folder->loading(); });
        folder->selectPaths({ pictures + QStringLiteral("/sunset #1.png") });
        QMetaObject::invokeMethod(window, "focusFiles");
        QTest::keyClick(window, Qt::Key_Space);
        auto *image = window->findChild<QQuickItem *>(QStringLiteral("previewImage"));
        check(image && waitFor([&] {
            return preview->property("opened").toBool() && image->property("status").toInt() == 1;
        }), "Space previews an image whose filename needs URL escaping");
        save(window, out + QStringLiteral("/preview-image.png"));
        auto *zoomIn = window->findChild<QQuickItem *>(QStringLiteral("previewZoomIn"));
        auto *zoomOut = window->findChild<QQuickItem *>(QStringLiteral("previewZoomOut"));
        auto *zoomFit = window->findChild<QQuickItem *>(QStringLiteral("previewZoomFit"));
        auto *imageView = window->findChild<QQuickItem *>(QStringLiteral("previewImageView"));
        if (check(zoomIn && zoomOut && zoomFit && imageView, "image preview has zoom controls")) {
            const qreal before = image->property("paintedWidth").toReal();
            QTest::mouseClick(window, Qt::LeftButton, {}, zoomIn->mapToScene(QPointF(22, 22)).toPoint());
            check(waitFor([&] {
                return image->property("status").toInt() == 1 && image->property("paintedWidth").toReal() > before
                    && imageView->property("contentHeight").toReal() > imageView->height();
            }), "zoom enlarges the image in a scrollable viewport");
            const qreal oldY = imageView->property("contentY").toReal();
            const QPoint middle = imageView->mapToScene(QPointF(imageView->width() / 2, imageView->height() / 2)).toPoint();
            QTest::mousePress(window, Qt::LeftButton, {}, middle);
            QTest::mouseMove(window, middle - QPoint(0, 20));
            QTest::mouseMove(window, middle - QPoint(0, 50));
            QTest::mouseRelease(window, Qt::LeftButton, {}, middle - QPoint(0, 50));
            check(waitFor([&] { return imageView->property("contentY").toReal() > oldY; }),
                  "dragging pans the zoomed image");
            QTest::mouseClick(window, Qt::LeftButton, {}, zoomFit->mapToScene(QPointF(zoomFit->width() / 2, 22)).toPoint());
            check(waitFor([&] {
                return preview->property("zoom").toReal() == 1 && !zoomOut->isEnabled()
                    && imageView->property("contentX").toReal() == 0;
            }), "Fit restores the image and resets panning");
            QWheelEvent wheel(middle, window->mapToGlobal(middle), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(window, &wheel);
            check(waitFor([&] { return preview->property("zoom").toReal() > 1; }), "Ctrl+wheel zooms the image");
            QTest::mouseClick(window, Qt::LeftButton, {}, zoomFit->mapToScene(QPointF(zoomFit->width() / 2, 22)).toPoint());
            zoomFit->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Space);
            check(waitFor([&] { return !preview->property("visible").toBool(); }),
                  "Space closes preview even when Fit has keyboard focus");
        }
        QTest::keyClick(window, Qt::Key_Escape);
        waitFor([&] { return !preview->property("visible").toBool(); });
        QTest::qWait(250);
        folder->go(docs);
        waitFor([&] { return !folder->loading(); });
        folder->selectPaths({ docs + QStringLiteral("/preview.docx") });
        QMetaObject::invokeMethod(window, "focusFiles");
        QTest::keyClick(window, Qt::Key_Space);
        check(waitFor([&] {
            return preview->property("content").toMap().value(QStringLiteral("kind")).toString() == QLatin1String("unsupported");
        }), "unsupported files show an explicit preview fallback");
        QTest::keyClick(window, Qt::Key_Escape);
        waitFor([&] { return !preview->property("visible").toBool(); });
        QTest::qWait(250);

        const QString pdfPath = docs + QStringLiteral("/preview #1.pdf");
        check(makePreviewPdf(pdfPath), "created a real two-page PDF fixture");
        folder->reload();
        waitFor([&] { return !folder->loading() && folder->rowOf(pdfPath) >= 0; });
        folder->selectPaths({ pdfPath });
        QMetaObject::invokeMethod(window, "focusFiles");
        QTest::keyClick(window, Qt::Key_Space);
        check(waitFor([&] {
            const auto content = preview->property("content").toMap();
            return content.value(QStringLiteral("kind")).toString() == QLatin1String("pdf")
                && content.value(QStringLiteral("pages")).toInt() == 2
                && image && image->property("status").toInt() == 1;
        }), "Space previews the first PDF page and reports its page count");
        save(window, out + QStringLiteral("/preview-pdf-1.png"));
        if (zoomIn && zoomOut && zoomFit) {
            check(preview->property("zoom").toReal() == 1, "a newly opened PDF starts fitted");
            QTest::keyClick(window, Qt::Key_Plus);
            check(waitFor([&] { return preview->property("zoom").toReal() > 1; }), "Plus zooms the PDF");
            save(window, out + QStringLiteral("/preview-pdf-zoom.png"));
            check(QMetaObject::invokeMethod(preview, "setZoom", Q_ARG(double, 10.0))
                      && preview->property("zoom").toReal() == 4 && !zoomIn->isEnabled(),
                  "zoom is bounded at four times fit");
            QTest::keyClick(window, Qt::Key_Minus);
            check(preview->property("zoom").toReal() < 4, "Minus zooms the PDF out");
            QTest::keyClick(window, Qt::Key_0);
            check(waitFor([&] { return preview->property("zoom").toReal() == 1; }), "Zero fits the PDF again");
        }
        auto *next = window->findChild<QQuickItem *>(QStringLiteral("previewNext"));
        auto *previous = window->findChild<QQuickItem *>(QStringLiteral("previewPrevious"));
        if (check(next && previous && next->isEnabled() && !previous->isEnabled(),
                  "PDF page buttons respect the first-page boundary")) {
            QTest::mouseClick(window, Qt::LeftButton, {}, next->mapToScene(QPointF(22, 22)).toPoint());
            check(waitFor([&] {
                return preview->property("content").toMap().value(QStringLiteral("page")).toInt() == 2
                    && image->property("status").toInt() == 1 && !next->isEnabled();
            }), "Next renders the second PDF page and disables at the last page");
            save(window, out + QStringLiteral("/preview-pdf-2.png"));
            QTest::mouseClick(window, Qt::LeftButton, {}, previous->mapToScene(QPointF(22, 22)).toPoint());
            check(waitFor([&] {
                return preview->property("content").toMap().value(QStringLiteral("page")).toInt() == 1
                    && image->property("status").toInt() == 1 && !previous->isEnabled();
            }), "Previous renders the first PDF page again");
            QTest::keyClick(window, Qt::Key_Right);
            check(waitFor([&] {
                return preview->property("content").toMap().value(QStringLiteral("page")).toInt() == 2
                    && image->property("status").toInt() == 1 && !next->isEnabled();
            }), "Right changes the PDF page from the keyboard");
            QTest::keyClick(window, Qt::Key_Left);
            check(waitFor([&] {
                return preview->property("content").toMap().value(QStringLiteral("page")).toInt() == 1
                    && image->property("status").toInt() == 1 && !previous->isEnabled();
            }), "Left changes the PDF page back");
        }
        QTest::keyClick(window, Qt::Key_Space);
        check(waitFor([&] { return !preview->property("visible").toBool(); }), "Space closes PDF preview");
    }

    // A stand-in xdg-mime changes only scratch data and never launches a real app.
    const QString mimeTools = home + QStringLiteral("/mime-tools");
    QDir().mkpath(mimeTools);
    const QString defaultState = mimeTools + QStringLiteral("/default");
    const QString mimeHelper = mimeTools + QStringLiteral("/xdg-mime");
    {
        QFile state(defaultState);
        if (state.open(QIODevice::WriteOnly))
            state.write("pfm-fake.desktop\n");
        QFile helper(mimeHelper);
        if (helper.open(QIODevice::WriteOnly))
            helper.write("#!/bin/sh\nif [ \"$1\" = query ]; then /bin/cat '" + defaultState.toUtf8()
                         + "'; elif [ \"$1\" = default ]; then printf '%s\\n' \"$2\" > '" + defaultState.toUtf8() + "'; fi\n");
        helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QFile second(home + QStringLiteral("/.local/share/applications/pfm-second.desktop"));
        if (second.open(QIODevice::WriteOnly))
            second.write("[Desktop Entry]\nType=Application\nName=Second Editor\nExec=/bin/true %f\nMimeType=text/plain;\n");
    }
    qputenv("PATH", QFile::encodeName(mimeTools) + ':' + qgetenv("PATH"));
    QTest::qWait(250);
    QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
    QObject *settings = window->findChild<QObject *>(QStringLiteral("settings"));
    check(settings && waitFor([&] { return settings->property("opened").toBool(); }),
          "Ctrl+comma opens Settings with default applications");
    if (settings) {
        auto *manager = settings->property("manager").value<MimeSettings *>();
        check(manager && waitFor([&] {
            return !manager->busy() && manager->info().value(QStringLiteral("currentName")).toString() == QLatin1String("Fake Editor");
        }), "Settings shows the effective xdg-mime default");
        settings->setProperty("chosen", QStringLiteral("pfm-second.desktop"));
        QFile state(defaultState);
        check(state.open(QIODevice::ReadOnly) && state.readAll() == QByteArray("pfm-fake.desktop\n"),
              "choosing an application does not save it yet");
        state.close();
        auto *apply = window->findChild<QQuickItem *>(QStringLiteral("settingsApply"));
        if (apply && manager) {
            QTest::mouseClick(window, Qt::LeftButton, {}, apply->mapToScene(QPointF(apply->width() / 2, 22)).toPoint());
            check(waitFor([&] {
                return !manager->busy() && manager->error().isEmpty()
                    && manager->info().value(QStringLiteral("currentId")).toString() == QLatin1String("pfm-second.desktop");
            }), "Apply updates and verifies the desktop default without launching the app");
        }
        save(window, out + QStringLiteral("/settings-defaults.png"));
        auto *typeList = window->findChild<QQuickItem *>(QStringLiteral("settingsTypes"));
        if (typeList) {
            typeList->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Down);
            QTest::keyClick(window, Qt::Key_Return);
            check(manager && waitFor([&] {
                return !manager->busy() && manager->info().value(QStringLiteral("mime")).toString() == QLatin1String("text/markdown");
            }), "type-list arrows and Enter inspect the keyboard choice");
        }
        auto *searchField = window->findChild<QQuickItem *>(QStringLiteral("settingsSearch"));
        if (searchField) {
            searchField->forceActiveFocus();
            for (const char key : QByteArray("application/pdf"))
                QTest::keyClick(window, key);
        }
        check(settings->property("query").toString() == QLatin1String("application/pdf"), "Settings searches MIME types");
        QTest::keyClick(window, Qt::Key_Return);
        check(manager && waitFor([&] {
            return !manager->busy() && manager->info().value(QStringLiteral("mime")).toString() == QLatin1String("application/pdf");
        }), "Enter inspects the matching MIME type");
        save(window, out + QStringLiteral("/settings-search.png"));
        QTest::keyClick(window, Qt::Key_Escape);
        check(waitFor([&] { return !settings->property("visible").toBool(); }), "Escape closes Settings");
    }

    std::printf("%s\n", failures ? "some checks failed" : "all checks passed");
    return failures ? 1 : 0;
}
