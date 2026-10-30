#include "FileOps.h"

#include "Format.h"
#include "Places.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeData>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

#include <sys/stat.h>

using Transfer::join;
using Transfer::present;

namespace {

constexpr int UndoSteps = 10;

// the clipboard formats for files: GNOME's (Nautilus) says copy or cut in its first line, KDE's
// (Dolphin) in a format of its own next to the plain list of URLs
constexpr auto GnomeFiles = "x-special/gnome-copied-files";
constexpr auto KdeCut = "application/x-kde-cutselection";

QString parentOf(const QString &path)
{
    const qsizetype at = path.lastIndexOf(u'/');
    return at <= 0 ? QStringLiteral("/") : path.left(at);
}

QString nameOf(const QString &path)
{
    return path.mid(path.lastIndexOf(u'/') + 1);
}

// a rename that won't replace anything; plain rename(2) where the drive can't promise that
bool move(const QString &from, const QString &to)
{
    if (QFile::rename(from, to))
        return true;
    return !present(to) && QDir().rename(from, to);
}

// a trashed item's .trashinfo, next to the trash's files/ folder
QString infoFileOf(const QString &trashedPath)
{
    const QString files = parentOf(trashedPath);
    if (nameOf(files) != QLatin1String("files"))
        return {};
    return parentOf(files) + QStringLiteral("/info/") + nameOf(trashedPath) + QStringLiteral(".trashinfo");
}

// where a trashed item came from, from its .trashinfo (relative paths are from the drive's top)
QString originalPath(const QString &trashedPath)
{
    QFile info(infoFileOf(trashedPath));
    if (!info.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!info.atEnd()) {
        const QByteArray line = info.readLine().trimmed();
        if (!line.startsWith("Path="))
            continue;
        const QString path = QUrl::fromPercentEncoding(line.mid(5));
        if (path.startsWith(u'/'))
            return QDir::cleanPath(path);
        // $topdir/.Trash-$uid or $topdir/.Trash/$uid
        const QString trashDir = parentOf(parentOf(trashedPath));
        QString top = parentOf(trashDir);
        if (nameOf(top) == QLatin1String(".Trash"))
            top = parentOf(top);
        return QDir::cleanPath(top + u'/' + path);
    }
    return {};
}

bool sameDrive(const QString &a, const QString &b)
{
    struct stat first, second;
    return ::lstat(QFile::encodeName(a).constData(), &first) == 0 && ::stat(QFile::encodeName(b).constData(), &second) == 0
        && first.st_dev == second.st_dev;
}

const QString NotIntoTrash = QStringLiteral("Only Move to trash puts things in the trash");

QString countedFailure(const QString &verb, const QStringList &failures)
{
    if (failures.size() == 1)
        return QStringLiteral("Couldn't %1 “%2”").arg(verb, nameOf(failures.first()));
    return QStringLiteral("Couldn't %1 %2 items").arg(verb).arg(failures.size());
}

} // namespace

FileOps::FileOps(QObject *parent)
    : QObject(parent)
{
    m_transfers.setMaxThreadCount(1);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &FileOps::readClipboard);
    readClipboard();
}

FileOps::~FileOps()
{
    *m_stop = true;
    m_transfers.waitForDone();
}

void FileOps::open(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            emit failed(QStringLiteral("Nothing here opens “%1”").arg(nameOf(path)));
    }
}

QString FileOps::checkName(const QString &dir, const QString &name, const QString &except) const
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("Type a name");
    if (name.contains(u'/'))
        return QStringLiteral("A name can't contain “/”");
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return QStringLiteral("“.” and “..” can't be names");
    if (name.toUtf8().size() > 255)
        return QStringLiteral("That name is too long");
    const QString target = join(dir, name);
    if (target != except && present(target))
        return QStringLiteral("Something called “%1” is already here").arg(name);
    return {};
}

QString FileOps::rename(const QString &path, const QString &name)
{
    if (name == nameOf(path))
        return {};
    const QString dir = parentOf(path);
    const QString problem = checkName(dir, name, path);
    if (!problem.isEmpty())
        return problem;
    if (!move(path, join(dir, name)))
        return QStringLiteral("Couldn't rename it (no permission here?)");
    remember({ Step::Renamed, { { path, join(dir, name) } }, {} });
    return {};
}

QString FileOps::makeFolder(const QString &dir, const QString &name)
{
    const QString problem = checkName(dir, name);
    if (!problem.isEmpty())
        return problem;
    if (!QDir(dir).mkdir(name))
        return QStringLiteral("Couldn't make a folder here (no permission?)");
    remember({ Step::MadeFolder, {}, { join(dir, name) } });
    return {};
}

bool FileOps::trash(const QStringList &paths)
{
    QList<std::pair<QString, QString>> trashed;
    const QStringList failures = trashAll(paths, &trashed);
    if (!trashed.isEmpty())
        remember({ Step::Trashed, trashed, {} });
    return failures.isEmpty();
}

bool FileOps::restore(const QStringList &trashedPaths)
{
    const QStringList restored = restoreAll(trashedPaths);
    if (!restored.isEmpty())
        remember({ Step::Restored, {}, restored });
    return restored.size() == trashedPaths.size();
}

// the failures; each one that went is added to `trashed` as (where it was, where it is in the trash)
QStringList FileOps::trashAll(const QStringList &paths, QList<std::pair<QString, QString>> *trashed)
{
    QStringList failures;
    for (const QString &path : paths) {
        QString inTrash;
        if (!QFile::moveToTrash(path, &inTrash))
            failures.append(path);
        else if (trashed)
            trashed->append({ path, inTrash });
    }
    if (!failures.isEmpty())
        emit failed(countedFailure(QStringLiteral("move to trash"), failures));
    return failures;
}

QStringList FileOps::restoreAll(const QStringList &trashedPaths)
{
    QStringList failures;
    QStringList restored;
    for (const QString &path : trashedPaths) {
        const QString original = originalPath(path);
        if (original.isEmpty()) {
            failures.append(path);
            continue;
        }
        const QString dir = parentOf(original);
        // something new took the name meanwhile: keep both
        const QString target = join(dir, Transfer::uniqueName(dir, nameOf(original)));
        if (!QDir().mkpath(dir) || !move(path, target)) {
            failures.append(path);
            continue;
        }
        QFile::remove(infoFileOf(path));
        restored.append(target);
    }
    if (!failures.isEmpty())
        emit failed(countedFailure(QStringLiteral("restore"), failures));
    return restored;
}

void FileOps::deleteForever(const QStringList &paths)
{
    started();
    const QPointer<FileOps> self(this);
    QThreadPool::globalInstance()->start([self, paths] {
        QStringList failures;
        for (const QString &path : paths) {
            if (!Transfer::removeAll(path)) {
                failures.append(path);
                continue;
            }
            const QString info = infoFileOf(path);
            if (!info.isEmpty())
                QFile::remove(info);
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, failures] {
            if (!self)
                return;
            self->ended();
            if (!failures.isEmpty())
                emit self->failed(countedFailure(QStringLiteral("delete"), failures));
            emit self->finished(QStringLiteral("delete"), {});
        }, Qt::QueuedConnection);
    });
}

void FileOps::emptyTrash()
{
    const QString trash = Places::homeTrash();
    QStringList paths;
    for (const QString &folder : { QStringLiteral("/files"), QStringLiteral("/info") }) {
        const QDir dir(trash + folder);
        const QStringList names = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString &name : names)
            paths.append(dir.filePath(name));
    }
    if (QFileInfo::exists(trash + QStringLiteral("/directorysizes")))
        paths.append(trash + QStringLiteral("/directorysizes"));
    deleteForever(paths);
}

void FileOps::copy(const QStringList &paths)
{
    if (!paths.isEmpty())
        setClipboard(paths, false);
}

void FileOps::cut(const QStringList &paths)
{
    if (!paths.isEmpty())
        setClipboard(paths, true);
}

void FileOps::paste(const QString &dir)
{
    readClipboard(); // another app may have changed it without telling (Wayland tells the focused window only)
    if (m_clipboard.isEmpty())
        return;
    if (Places::inTrashFolder(dir)) {
        emit failed(NotIntoTrash);
        return;
    }
    const QStringList paths = m_clipboard;
    const bool cut = m_clipboardCut;
    // what was cut is on its way: the clipboard lets go of it, as in other file managers
    if (cut)
        QGuiApplication::clipboard()->clear();
    put(paths, dir, cut);
}

void FileOps::drop(const QList<QUrl> &urls, const QString &dir)
{
    QStringList paths;
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths.append(QDir::cleanPath(url.toLocalFile()));
    }
    if (paths.isEmpty()) {
        emit failed(QStringLiteral("Only files can be dropped here"));
        return;
    }
    if (Places::isTrashFolder(dir)) {
        trash(paths);
        return;
    }
    if (Places::inTrashFolder(dir)) {
        emit failed(NotIntoTrash);
        return;
    }
    bool move = sameDrive(paths.first(), dir);
    // drop events carry no keys; Wayland tells them to the focused window, which a drag from here keeps
    const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    if (modifiers & Qt::ControlModifier)
        move = false;
    else if (modifiers & Qt::ShiftModifier)
        move = true;
    put(paths, dir, move);
}

void FileOps::put(const QStringList &paths, const QString &dir, bool move)
{
    QList<Transfer::Item> items;
    for (const QString &path : paths)
        items.append({ path, dir, nameOf(path) });
    transfer(items, move, [this, move](const Transfer::Result &result) {
        if (!result.intoItself.isEmpty())
            emit failed(QStringLiteral("“%1” can't go inside itself").arg(nameOf(result.intoItself.first())));
        if (!result.failures.isEmpty()) {
            const QString verb = move ? QStringLiteral("move") : QStringLiteral("copy");
            const QString what = countedFailure(verb, result.failures);
            emit failed(result.reason.isEmpty() ? what : what + QStringLiteral(": ") + result.reason);
        }
        QStringList landed;
        QList<std::pair<QString, QString>> moved;
        for (const auto &[from, to] : result.done) {
            landed.append(to);
            if (from != to)
                moved.append({ from, to });
        }
        if (move && !moved.isEmpty())
            remember({ Step::Moved, moved, {} });
        else if (!move && !landed.isEmpty())
            remember({ Step::Copied, {}, landed });
        emit finished(QStringLiteral("paste"), landed);
    });
}

void FileOps::copyPaths(const QStringList &paths)
{
    QGuiApplication::clipboard()->setText(paths.join(u'\n'));
}

void FileOps::compress(const QStringList &paths, const QString &dir)
{
    if (paths.isEmpty())
        return;
    const QString bsdtar = QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
    if (bsdtar.isEmpty()) {
        emit failed(QStringLiteral("Compress needs bsdtar (pacman -S libarchive)"));
        return;
    }

    QString base = QStringLiteral("Archive");
    if (paths.size() == 1) {
        const QFileInfo only(paths.first());
        base = only.isDir() || only.completeBaseName().isEmpty() ? only.fileName() : only.completeBaseName();
    }
    const QString archive = join(dir, Transfer::uniqueName(dir, base + QStringLiteral(".zip")));

    // -a picks the format from the name; "./" keeps names that start with "-" from reading as options
    QStringList args = { QStringLiteral("-a"), QStringLiteral("-cf"), archive, QStringLiteral("-C"), dir };
    for (const QString &path : paths)
        args.append(QStringLiteral("./") + nameOf(path));

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, archive](int code, QProcess::ExitStatus status) {
        ended();
        if (status != QProcess::NormalExit || code != 0) {
            QFile::remove(archive);
            const QString output = QString::fromLocal8Bit(process->readAll()).trimmed().section(u'\n', 0, 0);
            emit failed(QStringLiteral("Couldn't compress: %1").arg(output.isEmpty() ? QStringLiteral("bsdtar failed") : output));
        } else {
            remember({ Step::Compressed, {}, { archive } });
            emit finished(QStringLiteral("compress"), { archive });
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // the others end in finished()
        ended();
        emit failed(QStringLiteral("Couldn't start bsdtar"));
        process->deleteLater();
    });
    started();
    process->start(bsdtar, args);
}

void FileOps::undo()
{
    if (m_undo.isEmpty())
        return;
    const Step step = m_undo.takeLast();
    emit undoChanged();

    switch (step.kind) {
    case Step::Copied:
    case Step::Restored:
    case Step::Compressed:
        // made by this app, so they go where everything else goes: the trash
        if (trashAll(step.made).isEmpty()) {
            const char *what = step.kind == Step::Copied ? "copy" : step.kind == Step::Restored ? "restore" : "compress";
            emit notice(QStringLiteral("Undid %1").arg(QLatin1String(what)));
        }
        emit finished(QStringLiteral("undo"), {});
        break;

    case Step::MadeFolder: {
        const QString path = step.made.first();
        if (present(path) && !QDir().rmdir(path))
            emit failed(QStringLiteral("“%1” has things in it now, so it stays").arg(nameOf(path)));
        else
            emit notice(QStringLiteral("Undid new folder"));
        emit finished(QStringLiteral("undo"), {});
        break;
    }

    case Step::Renamed: {
        const auto &[before, after] = step.moves.first();
        if (present(before)) {
            emit failed(QStringLiteral("Couldn't undo the rename: something called “%1” is there now").arg(nameOf(before)));
            break;
        }
        if (!move(after, before)) {
            emit failed(QStringLiteral("Couldn't undo the rename"));
            break;
        }
        emit notice(QStringLiteral("Undid rename"));
        emit finished(QStringLiteral("undo"), { before });
        break;
    }

    case Step::Trashed: {
        QStringList trashed;
        for (const auto &[from, inTrash] : step.moves)
            trashed.append(inTrash);
        const QStringList restored = restoreAll(trashed);
        if (restored.size() == trashed.size())
            emit notice(QStringLiteral("Undid move to trash"));
        emit finished(QStringLiteral("undo"), restored);
        break;
    }

    case Step::Moved: {
        // back where each came from, under its old name; a drive in between copies again
        QList<Transfer::Item> items;
        for (const auto &[from, to] : step.moves)
            items.append({ to, parentOf(from), nameOf(from) });
        transfer(items, true, [this](const Transfer::Result &result) {
            if (!result.failures.isEmpty()) {
                const QString what = countedFailure(QStringLiteral("move back"), result.failures);
                emit failed(result.reason.isEmpty() ? what : what + QStringLiteral(": ") + result.reason);
            } else {
                emit notice(QStringLiteral("Undid move"));
            }
            QStringList landed;
            for (const auto &[from, to] : result.done)
                landed.append(to);
            emit finished(QStringLiteral("undo"), landed);
        });
        break;
    }
    }
}

QVariantMap FileOps::properties(const QString &path) const
{
    const QFileInfo info(path);
    const QLocale english(QLocale::English, QLocale::UnitedStates); // "2,400,000", like the rest of the UI's English
    const auto when = [&english](const QDateTime &time) {
        return time.isValid() ? english.toString(time, QStringLiteral("MMM d, yyyy, HH:mm")) : QString();
    };
    QString size;
    if (!info.isDir()) {
        size = Format::size(info.size());
        if (info.size() >= 1000)
            size += QStringLiteral(" (%1 bytes)").arg(english.toString(info.size()));
    }
    const QFile::Permissions permissions = info.permissions();
    QString rwx;
    for (const auto &[read, write, exe] : { std::tuple(QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner),
                                            std::tuple(QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup),
                                            std::tuple(QFile::ReadOther, QFile::WriteOther, QFile::ExeOther) })
        rwx += QLatin1String(permissions & read ? "r" : "-") + QLatin1String(permissions & write ? "w" : "-")
            + QLatin1String(permissions & exe ? "x" : "-");
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    return {
        { QStringLiteral("name"), info.fileName() },
        { QStringLiteral("kind"), info.isDir() ? QStringLiteral("folder") : Format::kind(mime) },
        { QStringLiteral("type"), info.isDir() ? QStringLiteral("Folder") : mime.comment() },
        { QStringLiteral("size"), size },
        { QStringLiteral("location"), displayPath(parentOf(path)) },
        { QStringLiteral("created"), when(info.birthTime()) },
        { QStringLiteral("modified"), when(info.lastModified()) },
        { QStringLiteral("permissions"), info.owner().isEmpty() ? rwx : rwx + QStringLiteral(" · ") + info.owner() },
        { QStringLiteral("link"), info.isSymLink() ? info.symLinkTarget() : QString() },
        { QStringLiteral("measure"), info.isDir() && !info.isSymLink() },
    };
}

void FileOps::measure(const QString &path)
{
    const QPointer<FileOps> self(this);
    // shortcut: a closed dialog doesn't stop the count, which only ties up a thread; add a stop flag if huge trees matter
    QThreadPool::globalInstance()->start([self, path] {
        Transfer::Progress total;
        Transfer::survey(path, total);
        const QString files = total.totalFiles == 1 ? QStringLiteral("1 file")
                                                    : QStringLiteral("%1 files").arg(QLocale(QLocale::English).toString(total.totalFiles));
        const QString size = Format::size(total.totalBytes) + QStringLiteral(" · ") + files;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, path, size] {
            if (self)
                emit self->measured(path, size);
        }, Qt::QueuedConnection);
    });
}

QString FileOps::displayPath(const QString &path) const
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + u'/'))
        return u'~' + path.mid(home.size());
    return path;
}

void FileOps::remember(const Step &step)
{
    m_undo.append(step);
    if (m_undo.size() > UndoSteps)
        m_undo.removeFirst();
    emit undoChanged();
}

void FileOps::setClipboard(const QStringList &paths, bool cut)
{
    QList<QUrl> urls;
    QByteArray gnome = cut ? "cut" : "copy";
    for (const QString &path : paths) {
        const QUrl url = QUrl::fromLocalFile(path);
        urls.append(url);
        gnome += '\n' + url.toEncoded();
    }
    auto *data = new QMimeData;
    data->setUrls(urls);
    data->setData(QLatin1String(GnomeFiles), gnome);
    data->setData(QLatin1String(KdeCut), cut ? "1" : "0");
    data->setText(paths.join(u'\n')); // a terminal gets the paths
    QGuiApplication::clipboard()->setMimeData(data);
    readClipboard(); // dataChanged may come later or, on some platforms, not for our own data
}

void FileOps::readClipboard()
{
    QStringList paths;
    bool cut = false;
    if (const QMimeData *data = QGuiApplication::clipboard()->mimeData()) {
        QList<QUrl> urls;
        if (data->hasFormat(QLatin1String(GnomeFiles))) {
            const QList<QByteArray> lines = data->data(QLatin1String(GnomeFiles)).split('\n');
            cut = lines.first().trimmed() == "cut";
            for (qsizetype i = 1; i < lines.size(); ++i) {
                if (!lines[i].trimmed().isEmpty())
                    urls.append(QUrl::fromEncoded(lines[i].trimmed()));
            }
        } else if (data->hasUrls()) {
            urls = data->urls();
            cut = data->data(QLatin1String(KdeCut)).startsWith('1');
        }
        for (const QUrl &url : std::as_const(urls)) {
            if (url.isLocalFile())
                paths.append(QDir::cleanPath(url.toLocalFile()));
        }
    }
    if (paths == m_clipboard && cut == m_clipboardCut)
        return;
    m_clipboard = paths;
    m_clipboardCut = cut;
    emit clipboardChanged();
}

// Runs a copy or move on the transfer thread; `then` gets the result back on this one.
void FileOps::transfer(const QList<Transfer::Item> &items, bool move, const std::function<void(const Transfer::Result &)> &then)
{
    started();
    const QPointer<FileOps> self(this);
    const std::shared_ptr<std::atomic<bool>> stop = m_stop;
    m_transfers.start([self, stop, items, move, then] {
        QElapsedTimer clock;
        clock.start();
        // quick ones never show a progress line
        const auto report = [self, &clock](const Transfer::Progress &progress) {
            if (clock.elapsed() < 400)
                return;
            QMetaObject::invokeMethod(QCoreApplication::instance(), [self, text = Transfer::describe(progress)] {
                if (self && self->m_running > 0)
                    self->setProgress(text);
            }, Qt::QueuedConnection);
        };
        const Transfer::Result result = Transfer::run(items, move, *stop, report);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, result, then] {
            if (!self)
                return;
            self->setProgress({});
            self->ended();
            then(result);
        }, Qt::QueuedConnection);
    });
}

void FileOps::setProgress(const QString &text)
{
    if (m_progress == text)
        return;
    m_progress = text;
    emit progressChanged();
}

void FileOps::started()
{
    if (m_running++ == 0)
        emit busyChanged();
}

void FileOps::ended()
{
    if (--m_running == 0)
        emit busyChanged();
}
