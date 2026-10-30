// Runs the real app on a made-up home laid out like the design boards' Documents folder, takes the
// boards' three pictures and a few more, and tries the mouse and keys on it: pfm-shots OUTDIR.
// Writes grid.png, list-menu.png, trash.png, trash-place.png, rename.png, cut.png, paste-menu.png, drag.png and properties.png,
// and prints a PASS or
// FAIL line per check (exit code 1 on a FAIL). Offscreen (QT_QPA_PLATFORM=offscreen) it doesn't
// touch the desktop; there, an icon of a row cut by the list's edge may show past the edge, which
// on a real display is clipped (checked on Hyprland).

#include "FileOps.h"
#include "FolderModel.h"
#include "Places.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFile>
#include <QGuiApplication>
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
    app.setDesktopFileName(QStringLiteral("pistachio-fm-shots"));
    QFontDatabase::families();

    QTemporaryDir root;
    const QString home = root.path() + QStringLiteral("/home");
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.local/share")).toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());

    // the boards' Documents (names, sizes and dates as drawn there)
    const QString docs = home + QStringLiteral("/Documents");
    for (const char *place : { "Documents", "Downloads", "Pictures", "Music" })
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
    for (const auto key : { Qt::Key_Down, Qt::Key_Down, Qt::Key_Return }) // New folder, Paste
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

    std::printf("%s\n", failures ? "some checks failed" : "all checks passed");
    return failures ? 1 : 0;
}
