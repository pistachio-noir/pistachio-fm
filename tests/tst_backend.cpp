#include "FileOps.h"
#include "FolderModel.h"
#include "Format.h"
#include "Places.h"
#include "Transfer.h"

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <sys/stat.h>

// The backend without a window. main() points HOME and XDG_DATA_HOME into a temporary folder
// first, so trashing never reaches the real trash.
class BackendTest : public QObject
{
    Q_OBJECT

public:
    explicit BackendTest(const QString &home)
        : m_home(home)
    {
    }

private:
    QString m_home;

    QString dir(const QString &name)
    {
        const QString path = m_home + u'/' + name;
        QDir(path).removeRecursively();
        QDir().mkpath(path);
        return path;
    }

    static void touch(const QString &path, qint64 size = 0, const QDateTime &modified = {})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(size));
        if (modified.isValid())
            QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
    }

    static void write(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    }

    static QByteArray contents(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
    }

    static QStringList names(const FolderModel &model)
    {
        QStringList list;
        for (int row = 0; row < model.rowCount(); ++row)
            list.append(model.data(model.index(row), FolderModel::NameRole).toString());
        return list;
    }

    static bool selected(const FolderModel &model, int row)
    {
        return model.data(model.index(row), FolderModel::SelectedRole).toBool();
    }

private slots:
    void sizes_data()
    {
        QTest::addColumn<qint64>("bytes");
        QTest::addColumn<QString>("text");
        QTest::newRow("zero") << qint64(0) << "0 B";
        QTest::newRow("bytes") << qint64(999) << "999 B";
        QTest::newRow("1 KB") << qint64(1000) << "1 KB";
        QTest::newRow("todo.md") << qint64(2048) << "2 KB";
        QTest::newRow("receipt") << qint64(88000) << "88 KB";
        QTest::newRow("rounds up to MB") << qint64(999600) << "1 MB";
        QTest::newRow("thesis") << qint64(2400000) << "2.4 MB";
        QTest::newRow("whole MB") << qint64(3000000) << "3 MB";
        QTest::newRow("mixtape") << qint64(34000000) << "34 MB";
        QTest::newRow("rounds to 10") << qint64(9960000) << "10 MB";
        QTest::newRow("free space") << qint64(212000000000) << "212 GB";
    }

    void sizes()
    {
        QFETCH(qint64, bytes);
        QFETCH(QString, text);
        QCOMPARE(Format::size(bytes), text);
    }

    void dates()
    {
        const QDateTime now(QDate(2026, 10, 29), QTime(20, 42));
        QCOMPARE(Format::date(QDateTime(QDate(2026, 10, 29), QTime(0, 5)), now), QStringLiteral("Today"));
        QCOMPARE(Format::date(QDateTime(QDate(2026, 10, 28), QTime(23, 0)), now), QStringLiteral("Yesterday"));
        QCOMPARE(Format::date(QDateTime(QDate(2026, 10, 27), QTime(9, 0)), now), QStringLiteral("Oct 27"));
        QCOMPARE(Format::date(QDateTime(QDate(2024, 3, 5), QTime(9, 0)), now), QStringLiteral("Mar 5, 2024"));
        QCOMPARE(Format::items(1), QStringLiteral("1 item"));
        QCOMPARE(Format::items(14), QStringLiteral("14 items"));
    }

    void kinds_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("kind");
        QTest::newRow("pdf") << "thesis-draft.pdf" << "document";
        QTest::newRow("markdown") << "todo.md" << "document";
        QTest::newRow("text") << "notes.txt" << "document";
        QTest::newRow("docx, a zip underneath") << "letter.docx" << "document";
        QTest::newRow("lua") << "init.lua" << "code";
        QTest::newRow("shell") << "install.sh" << "code";
        QTest::newRow("png") << "wallpaper-04.png" << "image";
        QTest::newRow("flac") << "mixtape.flac" << "audio";
        QTest::newRow("mkv") << "film.mkv" << "video";
        QTest::newRow("zip") << "boards-export.zip" << "archive";
        QTest::newRow("tar.gz") << "backup.tar.gz" << "archive";
        QTest::newRow("unknown") << "blob.qqqq" << "file";
    }

    void kinds()
    {
        QFETCH(QString, name);
        QFETCH(QString, kind);
        QCOMPARE(Format::kind(QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension)), kind);
    }

    void listing()
    {
        const QString path = dir(QStringLiteral("listing"));
        QDir().mkpath(path + QStringLiteral("/Projects"));
        touch(path + QStringLiteral("/Projects/a"));
        touch(path + QStringLiteral("/Projects/b"));
        QDir().mkpath(path + QStringLiteral("/notes"));
        touch(path + QStringLiteral("/file10.txt"));
        touch(path + QStringLiteral("/file2.txt"));
        touch(path + QStringLiteral("/Apple.md"), 2400000);

        FolderModel model;
        QVERIFY(model.go(path));
        QTRY_VERIFY(!model.loading());
        // folders first; names in natural order, case aside
        QCOMPARE(names(model), (QStringList { "notes", "Projects", "Apple.md", "file2.txt", "file10.txt" }));
        QCOMPARE(model.data(model.index(2), FolderModel::KindRole).toString(), QStringLiteral("document"));
        QCOMPARE(model.data(model.index(2), FolderModel::SizeTextRole).toString(), QStringLiteral("2.4 MB"));
        QCOMPARE(model.data(model.index(2), FolderModel::DetailRole).toString(), QStringLiteral("2.4 MB · Today"));
        // the folders' item counts come a moment later
        QTRY_COMPARE(model.data(model.index(1), FolderModel::DetailRole).toString(), QStringLiteral("2 items"));
        QTRY_COMPARE(model.data(model.index(0), FolderModel::SizeTextRole).toString(), QStringLiteral("0 items"));
    }

    void missingFolder()
    {
        FolderModel model;
        QSignalSpy failed(&model, &FolderModel::failed);
        QVERIFY(!model.go(m_home + QStringLiteral("/not-there")));
        QCOMPARE(failed.size(), 1);
        QVERIFY(model.path().isEmpty());
    }

    void fileOpensItsFolder()
    {
        const QString path = dir(QStringLiteral("file-arg"));
        touch(path + QStringLiteral("/a.txt"));
        touch(path + QStringLiteral("/b.txt"));
        FolderModel model;
        QVERIFY(model.go(path + QStringLiteral("/b.txt")));
        QTRY_VERIFY(!model.loading());
        QCOMPARE(model.path(), path);
        QCOMPARE(model.selectedPaths(), QStringList { path + QStringLiteral("/b.txt") });
    }

    void hiddenAndFilter()
    {
        const QString path = dir(QStringLiteral("hidden"));
        touch(path + QStringLiteral("/.hidden"));
        touch(path + QStringLiteral("/visible.txt"));
        touch(path + QStringLiteral("/Vision.png"));

        FolderModel model;
        model.go(path);
        QTRY_VERIFY(!model.loading());
        QCOMPARE(names(model), (QStringList { "visible.txt", "Vision.png" }));
        QCOMPARE(model.totalCount(), 2);

        model.setShowHidden(true);
        QCOMPARE(model.count(), 3);
        QCOMPARE(model.totalCount(), 3);

        model.setFilter(QStringLiteral("VIS"));
        QCOMPARE(names(model), (QStringList { "visible.txt", "Vision.png" }));
        QCOMPARE(model.totalCount(), 3);
    }

    void sorting()
    {
        const QString path = dir(QStringLiteral("sorting"));
        const QDateTime now = QDateTime::currentDateTime();
        QDir().mkpath(path + QStringLiteral("/z-folder"));
        touch(path + QStringLiteral("/a.txt"), 300, now.addDays(-3));
        touch(path + QStringLiteral("/b.txt"), 100, now.addDays(-1));
        touch(path + QStringLiteral("/c.png"), 200, now.addDays(-2));

        FolderModel model;
        model.go(path);
        QTRY_VERIFY(!model.loading());
        QCOMPARE(names(model), (QStringList { "z-folder", "a.txt", "b.txt", "c.png" }));
        model.setSortKey(FolderModel::BySize);
        QCOMPARE(names(model), (QStringList { "z-folder", "a.txt", "c.png", "b.txt" }));
        model.setSortKey(FolderModel::ByModified);
        QCOMPARE(names(model).mid(1), (QStringList { "b.txt", "c.png", "a.txt" }));
        model.setSortKey(FolderModel::ByType);
        QCOMPARE(names(model), (QStringList { "z-folder", "a.txt", "b.txt", "c.png" }));
    }

    void selection()
    {
        const QString path = dir(QStringLiteral("selection"));
        for (int i = 1; i <= 5; ++i)
            touch(path + QStringLiteral("/%1.txt").arg(i), i * 1000);

        FolderModel model;
        model.go(path);
        QTRY_VERIFY(!model.loading());
        QSignalSpy changed(&model, &FolderModel::selectionChanged);

        model.select(1, FolderModel::Replace);
        QCOMPARE(model.selectedCount(), 1);
        QVERIFY(selected(model, 1));
        model.select(3, FolderModel::Extend);
        QCOMPARE(model.selectedCount(), 3);
        QVERIFY(selected(model, 2));
        model.select(0, FolderModel::Toggle);
        QCOMPARE(model.selectedCount(), 4);
        QCOMPARE(model.currentRow(), 0);
        model.select(2, FolderModel::Toggle);
        QCOMPARE(model.selectedCount(), 3);
        QVERIFY(!selected(model, 2));
        QCOMPARE(model.selectedSize(), qint64(1000 + 2000 + 4000));
        QCOMPARE(model.summary().section(QStringLiteral(" · "), 0, 2), QStringLiteral("5 items · 3 selected · 7 KB"));

        model.clearSelection();
        QCOMPARE(model.selectedCount(), 0);
        QVERIFY(model.summary().startsWith(QStringLiteral("5 items · ")));
        QVERIFY(model.summary().endsWith(QStringLiteral(" free")));
        model.selectAll();
        QCOMPARE(model.selectedCount(), 5);
        QVERIFY(changed.size() >= 6);

        // the search filter drops what it hides from the selection
        model.setFilter(QStringLiteral("2"));
        QCOMPARE(model.selectedPaths(), QStringList { path + QStringLiteral("/2.txt") });
        QVERIFY(model.summary().startsWith(QStringLiteral("1 of 5 items · 1 selected")));
    }

    void history()
    {
        const QString root = dir(QStringLiteral("history"));
        QDir().mkpath(root + QStringLiteral("/a/b"));

        FolderModel model;
        model.go(root);
        model.go(root + QStringLiteral("/a"));
        model.go(QStringLiteral("b")); // relative to where it is
        QCOMPARE(model.path(), root + QStringLiteral("/a/b"));
        QVERIFY(model.canGoBack());
        QVERIFY(!model.canGoForward());

        model.back();
        QCOMPARE(model.path(), root + QStringLiteral("/a"));
        QVERIFY(model.canGoForward());
        QTRY_VERIFY(!model.loading());
        // coming back up selects the folder you were in
        QCOMPARE(model.selectedPaths(), QStringList { root + QStringLiteral("/a/b") });

        model.forward();
        QCOMPARE(model.path(), root + QStringLiteral("/a/b"));
        model.up();
        QCOMPARE(model.path(), root + QStringLiteral("/a"));
        QVERIFY(!model.canGoForward());

        model.go(QStringLiteral("~"));
        QCOMPARE(model.path(), QDir::homePath());
        model.go(QStringLiteral("/"));
        QVERIFY(!model.canGoUp());
    }

    void liveUpdate()
    {
        const QString path = dir(QStringLiteral("live"));
        touch(path + QStringLiteral("/a.txt"));
        touch(path + QStringLiteral("/b.txt"));

        FolderModel model;
        model.go(path);
        QTRY_VERIFY(!model.loading());
        model.select(1, FolderModel::Replace);
        QSignalSpy reset(&model, &FolderModel::modelReset);

        touch(path + QStringLiteral("/c.txt"));
        QTRY_COMPARE(model.count(), 3);
        QFile::remove(path + QStringLiteral("/a.txt"));
        QTRY_COMPARE(model.count(), 2);
        QCOMPARE(names(model), (QStringList { "b.txt", "c.txt" }));
        // rows moved in place: the selection stays and the views keep their scroll position
        QCOMPARE(model.selectedPaths(), QStringList { path + QStringLiteral("/b.txt") });
        QCOMPARE(reset.size(), 0);

        // the folder itself goes away: the model moves up to what's still there
        QDir(path).removeRecursively();
        QTRY_COMPARE(model.path(), m_home);
    }

    void renameAndMakeFolder()
    {
        const QString path = dir(QStringLiteral("names"));
        touch(path + QStringLiteral("/a.txt"));
        touch(path + QStringLiteral("/b.txt"));
        FileOps ops;

        QCOMPARE(ops.checkName(path, QString()), QStringLiteral("Type a name"));
        QVERIFY(ops.checkName(path, QStringLiteral("x/y")).contains(QStringLiteral("“/”")));
        QVERIFY(ops.checkName(path, QStringLiteral("..")).contains(QStringLiteral("can't be names")));
        QVERIFY(ops.checkName(path, QStringLiteral("b.txt")).contains(QStringLiteral("already here")));
        QVERIFY(ops.checkName(path, QStringLiteral("a.txt"), path + QStringLiteral("/a.txt")).isEmpty());

        QVERIFY(ops.rename(path + QStringLiteral("/a.txt"), QStringLiteral("b.txt")).contains(QStringLiteral("already here")));
        QVERIFY(QFile::exists(path + QStringLiteral("/a.txt")));
        QCOMPARE(ops.rename(path + QStringLiteral("/a.txt"), QStringLiteral("c.txt")), QString());
        QVERIFY(QFile::exists(path + QStringLiteral("/c.txt")));
        QVERIFY(!QFile::exists(path + QStringLiteral("/a.txt")));

        QCOMPARE(ops.makeFolder(path, QStringLiteral("Sub")), QString());
        QVERIFY(QFileInfo(path + QStringLiteral("/Sub")).isDir());
        QCOMPARE(ops.rename(path + QStringLiteral("/Sub"), QStringLiteral("Sub2")), QString());
        QVERIFY(QFileInfo(path + QStringLiteral("/Sub2")).isDir());
        QVERIFY(!ops.makeFolder(path, QStringLiteral("Sub2")).isEmpty());
        QCOMPARE(ops.displayPath(path + QStringLiteral("/Sub2")), QStringLiteral("~/names/Sub2"));
    }

    void trashAndRestore()
    {
        const QString path = dir(QStringLiteral("trashing"));
        const QString file = path + QStringLiteral("/doc.txt");
        touch(file, 10);
        FileOps ops;
        Places places;
        const QString trash = places.trash();
        QVERIFY2(trash.startsWith(m_home), qPrintable(trash)); // never the real one

        QVERIFY(ops.trash({ file }));
        QVERIFY(!QFile::exists(file));
        QVERIFY(QFile::exists(trash + QStringLiteral("/doc.txt")));
        QVERIFY(places.isTrash(trash));
        QVERIFY(places.inTrash(trash + QStringLiteral("/doc.txt")));

        QVERIFY(ops.restore({ trash + QStringLiteral("/doc.txt") }));
        QVERIFY(QFile::exists(file));
        QVERIFY(!QFile::exists(trash + QStringLiteral("/doc.txt")));
        QVERIFY(!QFile::exists(Places::homeTrash() + QStringLiteral("/info/doc.txt.trashinfo")));

        // something new took the name meanwhile: both are kept
        QVERIFY(ops.trash({ file }));
        touch(file, 20);
        QVERIFY(ops.restore({ trash + QStringLiteral("/doc.txt") }));
        QCOMPARE(QFileInfo(file).size(), 20);
        QCOMPARE(QFileInfo(path + QStringLiteral("/doc (2).txt")).size(), 10);
    }

    void deleteForever()
    {
        const QString path = dir(QStringLiteral("deleting"));
        const QString keepDir = dir(QStringLiteral("keep"));
        const QString keep = keepDir + QStringLiteral("/precious.txt");
        touch(keep);
        QDir().mkpath(path + QStringLiteral("/inner/deeper"));
        touch(path + QStringLiteral("/inner/deeper/x"));
        QVERIFY(QFile::link(keep, path + QStringLiteral("/inner/link")));
        QVERIFY(QFile::link(keepDir, path + QStringLiteral("/dirlink")));
        // a link handed over by itself goes, not what it points to
        QVERIFY(QFile::link(keepDir, m_home + QStringLiteral("/toplink")));

        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        ops.deleteForever({ path, m_home + QStringLiteral("/toplink") });
        QVERIFY(finished.wait(5000));
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFileInfo(m_home + QStringLiteral("/toplink")).isSymLink());
        QVERIFY(QFile::exists(keep)); // links are removed, never followed

        // emptying the trash
        const QString file = dir(QStringLiteral("to-empty")) + QStringLiteral("/gone.txt");
        touch(file);
        QVERIFY(ops.trash({ file }));
        ops.emptyTrash();
        QVERIFY(finished.wait(5000));
        QVERIFY(QDir(Places::homeTrash() + QStringLiteral("/files")).isEmpty());
        QVERIFY(QDir(Places::homeTrash() + QStringLiteral("/info")).isEmpty());
    }

    void compress()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("bsdtar")).isEmpty())
            QSKIP("bsdtar isn't installed");
        const QString path = dir(QStringLiteral("compressing"));
        touch(path + QStringLiteral("/-dash.txt"), 100);
        touch(path + QStringLiteral("/b.txt"), 100);
        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        ops.compress({ path + QStringLiteral("/-dash.txt"), path + QStringLiteral("/b.txt") }, path);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.first().at(1).toStringList(), QStringList { path + QStringLiteral("/Archive.zip") });
        QVERIFY(QFileInfo(path + QStringLiteral("/Archive.zip")).size() > 0);
        // a second one doesn't overwrite the first
        ops.compress({ path + QStringLiteral("/b.txt") }, path);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { path + QStringLiteral("/b.zip") });
        ops.compress({ path + QStringLiteral("/b.txt") }, path);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { path + QStringLiteral("/b (2).zip") });
        ops.undo();
        QVERIFY(!QFile::exists(path + QStringLiteral("/b (2).zip")));
        QVERIFY(QFile::exists(path + QStringLiteral("/b.zip")));
    }

    void clipboardFormats()
    {
        const QString path = dir(QStringLiteral("clip"));
        const QString file = path + QStringLiteral("/a b.txt");
        touch(file);
        const QByteArray url = QUrl::fromLocalFile(file).toEncoded();
        QClipboard *clipboard = QGuiApplication::clipboard();
        FileOps ops;

        ops.copy({ file });
        QVERIFY(ops.canPaste());
        QVERIFY(ops.cutPaths().isEmpty());
        QCOMPARE(clipboard->mimeData()->urls(), QList<QUrl> { QUrl::fromLocalFile(file) });
        QCOMPARE(clipboard->mimeData()->data(QStringLiteral("x-special/gnome-copied-files")), "copy\n" + url);
        QCOMPARE(clipboard->mimeData()->text(), file); // a terminal gets the path

        ops.cut({ file });
        QCOMPARE(ops.cutPaths(), QStringList { file });
        QCOMPARE(clipboard->mimeData()->data(QStringLiteral("x-special/gnome-copied-files")), "cut\n" + url);
        QCOMPARE(clipboard->mimeData()->data(QStringLiteral("application/x-kde-cutselection")), "1");

        // what other file managers put there: Nautilus's cut, Dolphin's copy
        auto *nautilus = new QMimeData;
        nautilus->setData(QStringLiteral("x-special/gnome-copied-files"), "cut\n" + url + "\n");
        clipboard->setMimeData(nautilus);
        QTRY_COMPARE(ops.cutPaths(), QStringList { file });
        auto *dolphin = new QMimeData;
        dolphin->setUrls({ QUrl::fromLocalFile(file) });
        dolphin->setData(QStringLiteral("application/x-kde-cutselection"), "0");
        clipboard->setMimeData(dolphin);
        QTRY_VERIFY(ops.cutPaths().isEmpty());
        QVERIFY(ops.canPaste());

        // text isn't files, even when it's a path
        clipboard->setText(file);
        QTRY_VERIFY(!ops.canPaste());
    }

    void pasteCopies()
    {
        const QString from = dir(QStringLiteral("paste-from"));
        const QString to = dir(QStringLiteral("paste-to"));
        const QDateTime old(QDate(2020, 1, 2), QTime(3, 4, 5));
        QDir().mkpath(from + QStringLiteral("/Folder/inner"));
        QDir().mkpath(from + QStringLiteral("/Folder/locked"));
        write(from + QStringLiteral("/Folder/.hidden"), "h");
        write(from + QStringLiteral("/Folder/inner/deep.txt"), "deep");
        write(from + QStringLiteral("/Folder/locked/x"), "x");
        write(from + QStringLiteral("/Folder/ro.txt"), "ro");
        QVERIFY(QFile::setPermissions(from + QStringLiteral("/Folder/ro.txt"), QFile::ReadOwner));
        QVERIFY(QFile::setPermissions(from + QStringLiteral("/Folder/locked"), QFile::ReadOwner | QFile::ExeOwner));
        QVERIFY(QFile::link(QStringLiteral("inner/deep.txt"), from + QStringLiteral("/Folder/rel-link")));
        touch(from + QStringLiteral("/note.txt"), 4, old);

        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        QSignalSpy failed(&ops, &FileOps::failed);
        ops.copy({ from + QStringLiteral("/Folder"), from + QStringLiteral("/note.txt") });
        ops.paste(to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(0).toString(), QStringLiteral("paste"));
        QCOMPARE(finished.last().at(1).toStringList(), (QStringList { to + QStringLiteral("/Folder"), to + QStringLiteral("/note.txt") }));
        QCOMPARE(failed.size(), 0);
        QCOMPARE(contents(to + QStringLiteral("/Folder/inner/deep.txt")), "deep");
        QCOMPARE(contents(to + QStringLiteral("/Folder/.hidden")), "h");
        QCOMPARE(contents(to + QStringLiteral("/Folder/locked/x")), "x");
        QCOMPARE(QFileInfo(to + QStringLiteral("/Folder/locked")).permissions(), QFileInfo(from + QStringLiteral("/Folder/locked")).permissions());
        QCOMPARE(QFileInfo(to + QStringLiteral("/Folder/ro.txt")).permissions(), QFileInfo(from + QStringLiteral("/Folder/ro.txt")).permissions());
        QCOMPARE(QFileInfo(to + QStringLiteral("/Folder/rel-link")).readSymLink(), QStringLiteral("inner/deep.txt")); // a link, as it was
        QCOMPARE(QFileInfo(to + QStringLiteral("/note.txt")).lastModified(), old);
        QCOMPARE(QFileInfo(to + QStringLiteral("/note.txt")).size(), 4);
        QVERIFY(QFile::exists(from + QStringLiteral("/note.txt"))); // a copy leaves the original
        QVERIFY(ops.canPaste()); // and stays on the clipboard

        // again: both are kept
        ops.paste(to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), (QStringList { to + QStringLiteral("/Folder (2)"), to + QStringLiteral("/note (2).txt") }));

        // into the folder it's in: a second one next to it
        ops.copy({ from + QStringLiteral("/note.txt") });
        ops.paste(from);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { from + QStringLiteral("/note (2).txt") });

        // a folder into itself
        ops.copy({ from + QStringLiteral("/Folder") });
        ops.paste(from + QStringLiteral("/Folder/inner"));
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.last().at(1).toStringList().isEmpty());
        QVERIFY2(failed.last().at(0).toString().contains(QStringLiteral("inside itself")), qPrintable(failed.last().at(0).toString()));
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/Folder/inner/Folder")));

        // nothing is pasted into the trash
        ops.paste(Places().trash());
        QVERIFY(failed.last().at(0).toString().contains(QStringLiteral("trash")));

        // something it can't read: that item leaves nothing behind, the others still land
        QDir().mkpath(from + QStringLiteral("/Bad"));
        write(from + QStringLiteral("/Bad/secret"), "s");
        QVERIFY(QFile::setPermissions(from + QStringLiteral("/Bad/secret"), {}));
        const QString fresh = dir(QStringLiteral("paste-fresh"));
        ops.copy({ from + QStringLiteral("/Bad"), from + QStringLiteral("/note.txt") });
        ops.paste(fresh);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { fresh + QStringLiteral("/note.txt") });
        QCOMPARE(failed.last().at(0).toString(), QStringLiteral("Couldn't copy “Bad”: no permission"));
        QVERIFY(!QFileInfo::exists(fresh + QStringLiteral("/Bad")));

        // so the temporary home can be cleaned up
        for (const QString &locked : { from + QStringLiteral("/Folder/locked"), to + QStringLiteral("/Folder/locked"),
                                       to + QStringLiteral("/Folder (2)/locked") })
            QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

    void cutAndPaste()
    {
        const QString from = dir(QStringLiteral("cut-from"));
        const QString to = dir(QStringLiteral("cut-to"));
        write(from + QStringLiteral("/a.txt"), "a");
        QDir().mkpath(from + QStringLiteral("/Sub"));
        write(from + QStringLiteral("/Sub/b.txt"), "b");
        write(to + QStringLiteral("/a.txt"), "taken");

        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        ops.cut({ from + QStringLiteral("/a.txt"), from + QStringLiteral("/Sub") });
        QCOMPARE(ops.cutPaths().size(), 2);
        ops.paste(to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), (QStringList { to + QStringLiteral("/a (2).txt"), to + QStringLiteral("/Sub") }));
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/a.txt")));
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/Sub")));
        QCOMPARE(contents(to + QStringLiteral("/a (2).txt")), "a");
        QCOMPARE(contents(to + QStringLiteral("/a.txt")), "taken");
        QCOMPARE(contents(to + QStringLiteral("/Sub/b.txt")), "b");
        // the clipboard lets go of what was cut
        QVERIFY(!ops.canPaste());
        QVERIFY(ops.cutPaths().isEmpty());

        // cut and pasted where it already is: nothing happens
        ops.cut({ to + QStringLiteral("/Sub") });
        ops.paste(to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { to + QStringLiteral("/Sub") });
        QVERIFY(!QFileInfo::exists(to + QStringLiteral("/Sub (2)")));
        QCOMPARE(contents(to + QStringLiteral("/Sub/b.txt")), "b");
    }

    void cutAcrossDrives()
    {
        // /dev/shm is a drive of its own next to the temporary home, so a move there copies, then deletes
        QTemporaryDir other(QStringLiteral("/dev/shm/pfm-test-XXXXXX"));
        if (!other.isValid())
            QSKIP("no /dev/shm here");
        struct stat home, shm;
        QVERIFY(::stat(QFile::encodeName(m_home).constData(), &home) == 0);
        QVERIFY(::stat(QFile::encodeName(other.path()).constData(), &shm) == 0);
        if (home.st_dev == shm.st_dev)
            QSKIP("/dev/shm is on the same drive as the temporary home");

        const QString from = other.path();
        QDir().mkpath(from + QStringLiteral("/Tree/x"));
        write(from + QStringLiteral("/Tree/x/y.txt"), "y");
        QVERIFY(QFile::link(QStringLiteral("x/y.txt"), from + QStringLiteral("/Tree/link")));
        const QString to = dir(QStringLiteral("from-another-drive"));

        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        QSignalSpy notice(&ops, &FileOps::notice);
        ops.cut({ from + QStringLiteral("/Tree") });
        ops.paste(to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { to + QStringLiteral("/Tree") });
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/Tree")));
        QCOMPARE(contents(to + QStringLiteral("/Tree/x/y.txt")), "y");
        QCOMPARE(QFileInfo(to + QStringLiteral("/Tree/link")).readSymLink(), QStringLiteral("x/y.txt"));

        // and back across with undo
        ops.undo();
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { from + QStringLiteral("/Tree") });
        QCOMPARE(notice.last().at(0).toString(), QStringLiteral("Undid move"));
        QCOMPARE(contents(from + QStringLiteral("/Tree/x/y.txt")), "y");
        QVERIFY(!QFileInfo::exists(to + QStringLiteral("/Tree")));
    }

    void dropping()
    {
        const QString from = dir(QStringLiteral("drop-from"));
        const QString to = dir(QStringLiteral("drop-to"));
        write(from + QStringLiteral("/a.txt"), "a");
        write(from + QStringLiteral("/b.txt"), "b");
        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        QSignalSpy failed(&ops, &FileOps::failed);

        // on the same drive, a move; undo takes it back
        ops.drop({ QUrl::fromLocalFile(from + QStringLiteral("/a.txt")) }, to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { to + QStringLiteral("/a.txt") });
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/a.txt")));
        ops.undo();
        QVERIFY(finished.wait(5000));
        QCOMPARE(contents(from + QStringLiteral("/a.txt")), "a");

        // onto the Trash place: trashed, as with Delete
        ops.drop({ QUrl::fromLocalFile(from + QStringLiteral("/b.txt")) }, Places().trash());
        QVERIFY(!QFileInfo::exists(from + QStringLiteral("/b.txt")));
        ops.undo();
        QCOMPARE(contents(from + QStringLiteral("/b.txt")), "b");

        // not into a folder inside the trash, and nothing but local files
        ops.drop({ QUrl::fromLocalFile(from + QStringLiteral("/b.txt")) }, Places().trash() + QStringLiteral("/old"));
        QVERIFY(failed.last().at(0).toString().contains(QStringLiteral("Move to trash")));
        ops.drop({ QUrl(QStringLiteral("https://example.com/x.png")) }, to);
        QCOMPARE(failed.last().at(0).toString(), QStringLiteral("Only files can be dropped here"));
        QCOMPARE(contents(from + QStringLiteral("/b.txt")), "b");

        // from another drive, a copy: the original stays
        QTemporaryDir other(QStringLiteral("/dev/shm/pfm-test-XXXXXX"));
        struct stat home, shm;
        if (!other.isValid() || ::stat(QFile::encodeName(m_home).constData(), &home) != 0
            || ::stat(QFile::encodeName(other.path()).constData(), &shm) != 0 || home.st_dev == shm.st_dev)
            QSKIP("no /dev/shm on a drive of its own here");
        write(other.path() + QStringLiteral("/c.txt"), "c");
        ops.drop({ QUrl::fromLocalFile(other.path() + QStringLiteral("/c.txt")) }, to);
        QVERIFY(finished.wait(5000));
        QCOMPARE(contents(to + QStringLiteral("/c.txt")), "c");
        QCOMPARE(contents(other.path() + QStringLiteral("/c.txt")), "c");
    }

    void undo()
    {
        const QString path = dir(QStringLiteral("undoing"));
        const QString other = dir(QStringLiteral("undoing-elsewhere"));
        write(path + QStringLiteral("/a.txt"), "a");
        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        QSignalSpy failed(&ops, &FileOps::failed);
        QSignalSpy notice(&ops, &FileOps::notice);
        QVERIFY(!ops.canUndo());

        // a new folder goes again...
        QCOMPARE(ops.makeFolder(path, QStringLiteral("New")), QString());
        QVERIFY(ops.canUndo());
        ops.undo();
        QVERIFY(!QFileInfo::exists(path + QStringLiteral("/New")));
        QCOMPARE(notice.last().at(0).toString(), QStringLiteral("Undid new folder"));
        QVERIFY(!ops.canUndo());
        // ...unless something went into it meanwhile
        QCOMPARE(ops.makeFolder(path, QStringLiteral("Kept")), QString());
        write(path + QStringLiteral("/Kept/x"), "x");
        ops.undo();
        QCOMPARE(contents(path + QStringLiteral("/Kept/x")), "x");
        QVERIFY(failed.last().at(0).toString().contains(QStringLiteral("stays")));

        // a rename
        QCOMPARE(ops.rename(path + QStringLiteral("/a.txt"), QStringLiteral("b.txt")), QString());
        ops.undo();
        QCOMPARE(contents(path + QStringLiteral("/a.txt")), "a");
        QVERIFY(!QFileInfo::exists(path + QStringLiteral("/b.txt")));
        QCOMPARE(finished.last().at(1).toStringList(), QStringList { path + QStringLiteral("/a.txt") });

        // a move to the trash
        QVERIFY(ops.trash({ path + QStringLiteral("/a.txt") }));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral("/a.txt")));
        ops.undo();
        QCOMPARE(contents(path + QStringLiteral("/a.txt")), "a");
        QCOMPARE(notice.last().at(0).toString(), QStringLiteral("Undid move to trash"));

        // a copy: the copies go to the trash, the original stays
        ops.copy({ path + QStringLiteral("/a.txt") });
        ops.paste(other);
        QVERIFY(finished.wait(5000));
        QCOMPARE(contents(other + QStringLiteral("/a.txt")), "a");
        ops.undo();
        QVERIFY(!QFileInfo::exists(other + QStringLiteral("/a.txt")));
        QCOMPARE(contents(path + QStringLiteral("/a.txt")), "a");
        QCOMPARE(notice.last().at(0).toString(), QStringLiteral("Undid copy"));

        // a move: back where it came from
        ops.cut({ path + QStringLiteral("/a.txt") });
        ops.paste(other);
        QVERIFY(finished.wait(5000));
        QCOMPARE(contents(other + QStringLiteral("/a.txt")), "a");
        ops.undo();
        QVERIFY(finished.wait(5000));
        QCOMPARE(contents(path + QStringLiteral("/a.txt")), "a");
        QVERIFY(!QFileInfo::exists(other + QStringLiteral("/a.txt")));
        QVERIFY(!ops.canUndo());

        // latest first, and only the last ten
        for (int i = 0; i < 12; ++i)
            QCOMPARE(ops.makeFolder(path, QString::number(i)), QString());
        for (int i = 0; i < 12; ++i)
            ops.undo();
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/0")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/1")));
        for (int i = 2; i < 12; ++i)
            QVERIFY(!QFileInfo::exists(path + u'/' + QString::number(i)));
        QVERIFY(!ops.canUndo());
    }

    void properties()
    {
        const QString path = dir(QStringLiteral("props"));
        const QDateTime old(QDate(2020, 1, 2), QTime(3, 4, 5));
        touch(path + QStringLiteral("/big.bin"), 2400000, old);
        write(path + QStringLiteral("/a.txt"), "abc");
        QVERIFY(QFile::setPermissions(path + QStringLiteral("/a.txt"),
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther));
        QDir().mkpath(path + QStringLiteral("/Folder/sub"));
        write(path + QStringLiteral("/Folder/x"), "xy");
        write(path + QStringLiteral("/Folder/sub/y"), "z");
        QVERIFY(QFile::link(QStringLiteral("x"), path + QStringLiteral("/Folder/link")));
        FileOps ops;

        QVariantMap info = ops.properties(path + QStringLiteral("/big.bin"));
        QCOMPARE(info.value(QStringLiteral("name")).toString(), QStringLiteral("big.bin"));
        QCOMPARE(info.value(QStringLiteral("size")).toString(), QStringLiteral("2.4 MB (2,400,000 bytes)"));
        QCOMPARE(info.value(QStringLiteral("modified")).toString(), QStringLiteral("Jan 2, 2020, 03:04"));
        QCOMPARE(info.value(QStringLiteral("location")).toString(), QStringLiteral("~/props"));
        QVERIFY(!info.value(QStringLiteral("measure")).toBool());

        info = ops.properties(path + QStringLiteral("/a.txt"));
        QCOMPARE(info.value(QStringLiteral("size")).toString(), QStringLiteral("3 B"));
        QCOMPARE(info.value(QStringLiteral("type")).toString(), QMimeDatabase().mimeTypeForName(QStringLiteral("text/plain")).comment());
        QVERIFY2(info.value(QStringLiteral("permissions")).toString().startsWith(QStringLiteral("rw-r--r-- · ")),
                 qPrintable(info.value(QStringLiteral("permissions")).toString()));
        QCOMPARE(ops.properties(path + QStringLiteral("/Folder/link")).value(QStringLiteral("link")).toString(),
                 path + QStringLiteral("/Folder/x"));

        // a folder's size is counted in the background; links count as files, never followed
        info = ops.properties(path + QStringLiteral("/Folder"));
        QCOMPARE(info.value(QStringLiteral("type")).toString(), QStringLiteral("Folder"));
        QVERIFY(info.value(QStringLiteral("measure")).toBool());
        QSignalSpy measured(&ops, &FileOps::measured);
        ops.measure(path + QStringLiteral("/Folder"));
        QVERIFY(measured.wait(5000));
        QCOMPARE(measured.first().at(0).toString(), path + QStringLiteral("/Folder"));
        QCOMPARE(measured.first().at(1).toString(), QStringLiteral("3 B · 3 files"));
    }

    void progressShows()
    {
        // enough small files that copying them outlasts the 400 ms before a progress line shows
        const QString from = dir(QStringLiteral("many"));
        const QString to = dir(QStringLiteral("many-copied"));
        QDir().mkpath(from + QStringLiteral("/Many"));
        for (int i = 0; i < 40000; ++i)
            write(from + QStringLiteral("/Many/%1").arg(i), "x");
        FileOps ops;
        QSignalSpy finished(&ops, &FileOps::finished);
        QStringList lines;
        connect(&ops, &FileOps::progressChanged, this, [&] { lines.append(ops.progress()); });
        QElapsedTimer clock;
        clock.start();
        ops.copy({ from + QStringLiteral("/Many") });
        ops.paste(to);
        QVERIFY(finished.wait(60000));
        QCOMPARE(QDir(to + QStringLiteral("/Many")).count(), 40000 + 2);
        if (clock.elapsed() < 600)
            QSKIP("this machine copied them before a progress line was due");
        QVERIFY(lines.size() >= 2);
        QVERIFY2(lines.first().startsWith(QStringLiteral("Copying ")) && lines.first().endsWith(QStringLiteral(" of 40 KB")),
                 qPrintable(lines.first()));
        QVERIFY2(lines.first().contains(QStringLiteral("“Many” · ")), qPrintable(lines.first())); // one item: its name
        QCOMPARE(lines.last(), QString()); // gone when it's done
        QVERIFY(ops.progress().isEmpty());
        QDir(from).removeRecursively();
        QDir(to).removeRecursively();
    }

    void progressLine()
    {
        Transfer::Progress progress;
        progress.files = 2;
        progress.totalFiles = 14;
        progress.bytes = 340000000;
        progress.totalBytes = 1200000000;
        QCOMPARE(Transfer::describe(progress), QStringLiteral("Copying 3 of 14 · 340 MB of 1.2 GB"));
        progress.moving = true;
        progress.name = QStringLiteral("disk.iso");
        QCOMPARE(Transfer::describe(progress), QStringLiteral("Moving “disk.iso” · 340 MB of 1.2 GB"));
    }

    void removableDrives()
    {
        // stand-ins for lsblk and udiskie, first on PATH: a stick with a labelled partition, one without a
        // label, an encrypted one, the stick itself (no filesystem), and the computer's own disk
        QTemporaryDir bin;
        const auto script = [&](const char *name, const QByteArray &body) {
            const QString path = bin.path() + u'/' + QLatin1String(name);
            write(path, "#!/bin/sh\n" + body);
            QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        };
        script("lsblk", "cat <<'EOF'\n{\"blockdevices\": [\n"
                        " {\"path\":\"/dev/sda2\", \"label\":null, \"fstype\":\"ext4\", \"mountpoint\":null, \"hotplug\":false, \"rm\":false, \"size\":500000000000},\n"
                        " {\"path\":\"/dev/sdz1\", \"label\":\"Stick\", \"fstype\":\"exfat\", \"mountpoint\":null, \"hotplug\":true, \"rm\":true, \"size\":32000000000},\n"
                        " {\"path\":\"/dev/sdz2\", \"label\":null, \"fstype\":\"vfat\", \"mountpoint\":null, \"hotplug\":true, \"rm\":false, \"size\":32000000},\n"
                        " {\"path\":\"/dev/sdz3\", \"label\":null, \"fstype\":\"crypto_LUKS\", \"mountpoint\":null, \"hotplug\":true, \"rm\":true, \"size\":1000},\n"
                        " {\"path\":\"/dev/sdz4\", \"label\":\"Used\", \"fstype\":\"ext4\", \"mountpoint\":\"/somewhere\", \"hotplug\":true, \"rm\":true, \"size\":1000},\n"
                        " {\"path\":\"/dev/sdz\", \"label\":null, \"fstype\":null, \"mountpoint\":null, \"hotplug\":true, \"rm\":true, \"size\":32000000000}\n"
                        "]}\nEOF\n");
        script("udiskie-mount", "echo \"failed to add $1: wrong fs type\" >&2; exit 1\n");
        script("udiskie-umount", "echo \"failed to remove $1: target is busy\" >&2; exit 1\n");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", bin.path().toLocal8Bit() + ':' + path);

        Places places;
        QSignalSpy failed(&places, &Places::failed);
        const auto unmounted = [&] {
            QStringList list;
            for (const QVariant &device : places.devices()) {
                const QVariantMap map = device.toMap();
                if (map.value(QStringLiteral("path")).toString().isEmpty())
                    list.append(map.value(QStringLiteral("name")).toString() + u' ' + map.value(QStringLiteral("device")).toString());
            }
            return list;
        };
        QTRY_COMPARE(unmounted(), (QStringList { QStringLiteral("Stick /dev/sdz1"), QStringLiteral("32 MB drive /dev/sdz2") }));
        QCOMPARE(places.devices().last().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Trash"));

        places.mount(QStringLiteral("/dev/sdz1"));
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.last().at(0).toString(), QStringLiteral("Couldn't mount it: wrong fs type"));
        places.unmount(QStringLiteral("/dev/sdz1"));
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.last().at(0).toString(), QStringLiteral("The drive is in use: close what's open on it, then unmount it again"));
        qputenv("PATH", path);
    }

    void crumbsAndPlaces()
    {
        QDir().mkpath(m_home + QStringLiteral("/Documents"));
        Places places;
        const auto crumbNames = [&](const QString &path) {
            QStringList list;
            for (const QVariant &crumb : places.crumbs(path))
                list.append(crumb.toMap().value(QStringLiteral("name")).toString());
            return list;
        };
        QCOMPARE(crumbNames(m_home), QStringList { "Home" });
        QCOMPARE(crumbNames(m_home + QStringLiteral("/Documents/x")), (QStringList { "Home", "Documents", "x" }));
        QCOMPARE(crumbNames(QStringLiteral("/usr/share")), (QStringList { "System", "usr", "share" }));
        QCOMPARE(crumbNames(QStringLiteral("/")), QStringList { "System" });
        QCOMPARE(crumbNames(places.trash() + QStringLiteral("/old")), (QStringList { "Trash", "old" }));
        QCOMPARE(places.crumbs(m_home + QStringLiteral("/Documents")).last().toMap().value(QStringLiteral("path")).toString(),
                 m_home + QStringLiteral("/Documents"));

        QCOMPARE(places.placeFor(m_home + QStringLiteral("/Documents/x")), m_home + QStringLiteral("/Documents"));
        QCOMPARE(places.placeFor(m_home + QStringLiteral("/Projects")), m_home);
        QCOMPARE(places.placeFor(QStringLiteral("/etc")), QStringLiteral("/"));
        QCOMPARE(places.placeFor(places.trash()), places.trash());
        QVERIFY(!places.isTrash(places.trash() + QStringLiteral("/old")));
        QVERIFY(places.isTrash(QStringLiteral("/run/media/me/USB/.Trash-1000/files")));
        QVERIFY(places.inTrash(QStringLiteral("/run/media/me/USB/.Trash-1000/files/a/b")));
        QVERIFY(!places.inTrash(QStringLiteral("/run/media/me/USB/files")));
    }
};

int main(int argc, char *argv[])
{
    QTemporaryDir root;
    const QString home = root.path() + QStringLiteral("/home");
    QDir().mkpath(home);
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.local/share")).toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    BackendTest test(home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_backend.moc"
