#include "Places.h"

#include "Format.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>

namespace {

QVariantMap entry(const QString &name, const QString &path, const QString &icon)
{
    return { { QStringLiteral("name"), name }, { QStringLiteral("path"), path }, { QStringLiteral("icon"), icon } };
}

bool holds(const QString &root, const QString &path)
{
    return path == root || root == QLatin1String("/") || path.startsWith(root + u'/');
}

// drives mounted for the user (udisks puts them in /run/media/$USER), not the system's own mounts
QVariantList volumes()
{
    QVariantList list;
    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : mounted) {
        const QString root = storage.rootPath();
        if (!storage.isValid() || !storage.isReady())
            continue;
        if (!root.startsWith(QLatin1String("/run/media/")) && !root.startsWith(QLatin1String("/media/"))
            && !root.startsWith(QLatin1String("/mnt/")))
            continue;
        const QString name = storage.name().isEmpty() ? QFileInfo(root).fileName() : storage.name();
        QVariantMap volume = entry(name, root, QStringLiteral("drive"));
        volume.insert(QStringLiteral("device"), QString::fromLocal8Bit(storage.device())); // for unmounting
        list.append(volume);
    }
    return list;
}

// partitions of removable drives that have a filesystem but no mount, from `lsblk -J -l`; no path yet,
// their device to mount
QVariantList unmountedDrives(const QByteArray &json)
{
    QVariantList list;
    const QJsonArray devices = QJsonDocument::fromJson(json).object().value(QStringLiteral("blockdevices")).toArray();
    for (const QJsonValue &value : devices) {
        const QJsonObject device = value.toObject();
        const QString fs = device.value(QStringLiteral("fstype")).toString();
        // shortcut: encrypted drives are left out, udiskie would have to ask for their password; add when wanted
        if (!(device.value(QStringLiteral("hotplug")).toBool() || device.value(QStringLiteral("rm")).toBool())
            || !device.value(QStringLiteral("mountpoint")).isNull() || fs.isEmpty() || fs == QLatin1String("swap")
            || fs == QLatin1String("crypto_LUKS"))
            continue;
        const QString label = device.value(QStringLiteral("label")).toString();
        QVariantMap drive = entry(label.isEmpty() ? Format::size(device.value(QStringLiteral("size")).toInteger()) + QStringLiteral(" drive") : label,
                                  {}, QStringLiteral("drive"));
        drive.insert(QStringLiteral("device"), device.value(QStringLiteral("path")).toString());
        list.append(drive);
    }
    return list;
}

} // namespace

Places::Places(QObject *parent)
    : QObject(parent)
{
    m_places.append(entry(QStringLiteral("Home"), home(), QStringLiteral("home")));
    const struct {
        QStandardPaths::StandardLocation location;
        const char *name;
        const char *icon;
    } standard[] = {
        { QStandardPaths::DesktopLocation, "Desktop", "monitor" },
        { QStandardPaths::DocumentsLocation, "Documents", "fileText" },
        { QStandardPaths::DownloadLocation, "Downloads", "download" },
        { QStandardPaths::PicturesLocation, "Pictures", "image" },
        { QStandardPaths::MusicLocation, "Music", "music" },
    };
    for (const auto &place : standard) {
        const QString path = QStandardPaths::writableLocation(place.location);
        // unset XDG folders fall back to home itself; missing ones would only fail to open
        if (path != home() && QFileInfo(path).isDir())
            m_places.append(entry(QLatin1String(place.name), path, QLatin1String(place.icon)));
    }

    // last of the places, so drives plugged in never push it out of view
    m_places.append(entry(QStringLiteral("Trash"), trash(), QStringLiteral("trash")));
    QDir().mkpath(trash()); // so the Trash place opens before anything was ever trashed
    connect(&m_lsblk, &QProcess::finished, this, [this] {
        m_unmounted = unmountedDrives(m_lsblk.readAllStandardOutput());
        showDevices();
    });
    refreshDevices();
    // mounting is rare; reading the mount table every few seconds costs nothing
    m_poll.setInterval(4000);
    connect(&m_poll, &QTimer::timeout, this, &Places::refreshDevices);
    // and so are bookmarks Nautilus adds or removes
    connect(&m_poll, &QTimer::timeout, this, &Places::readBookmarks);
    readBookmarks();
    m_poll.start();
}

QString Places::bookmarksFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/gtk-3.0/bookmarks");
}

// "file:///home/me/My%20Projects Label": the folder, and the name to show (its own unless labelled)
static QString bookmarkPath(const QByteArray &line, QString *name = nullptr)
{
    const qsizetype space = line.indexOf(' ');
    const QUrl url = QUrl::fromEncoded(line.left(space < 0 ? line.size() : space));
    if (!url.isLocalFile())
        return {}; // sftp:// and the like: kept in the file, not shown
    const QString path = QDir::cleanPath(url.toLocalFile());
    if (name) {
        const QString label = space < 0 ? QString() : QString::fromUtf8(line.mid(space + 1)).trimmed();
        *name = !label.isEmpty() ? label : path == u'/' ? QStringLiteral("System") : QFileInfo(path).fileName();
    }
    return path;
}

static QList<QByteArray> bookmarkLines()
{
    QFile file(Places::bookmarksFile());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QList<QByteArray> lines = file.readAll().split('\n');
    lines.removeAll(QByteArray());
    return lines;
}

void Places::readBookmarks()
{
    QVariantList bookmarks;
    for (const QByteArray &line : bookmarkLines()) {
        QString name;
        const QString path = bookmarkPath(line, &name);
        if (!path.isEmpty())
            bookmarks.append(QVariantMap { { QStringLiteral("name"), name }, { QStringLiteral("path"), path },
                                           { QStringLiteral("icon"), QStringLiteral("folder") } });
    }
    if (bookmarks == m_bookmarks)
        return;
    m_bookmarks = bookmarks;
    emit bookmarksChanged();
}

bool Places::writeBookmarks(const QList<QByteArray> &lines)
{
    QDir().mkpath(QFileInfo(bookmarksFile()).path());
    QSaveFile file(bookmarksFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(lines.join('\n') + '\n') < 0 || !file.commit()) {
        emit failed(QStringLiteral("Couldn't save the bookmarks in ~/.config/gtk-3.0/bookmarks"));
        return false;
    }
    readBookmarks();
    return true;
}

bool Places::isBookmarked(const QString &path) const
{
    for (const QVariant &bookmark : m_bookmarks) {
        if (bookmark.toMap().value(QStringLiteral("path")).toString() == QDir::cleanPath(path))
            return true;
    }
    return false;
}

bool Places::addBookmark(const QString &path)
{
    if (!QFileInfo(path).isDir()) {
        emit failed(QStringLiteral("Only folders can be bookmarked"));
        return false;
    }
    if (isBookmarked(path))
        return true;
    return writeBookmarks(bookmarkLines() << QUrl::fromLocalFile(QDir::cleanPath(path)).toEncoded());
}

void Places::addBookmarks(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            addBookmark(url.toLocalFile());
    }
}

void Places::removeBookmark(const QString &path)
{
    QList<QByteArray> lines = bookmarkLines();
    const qsizetype before = lines.size();
    lines.removeIf([&](const QByteArray &line) { return bookmarkPath(line) == QDir::cleanPath(path); });
    if (lines.size() != before)
        writeBookmarks(lines);
}

Places::~Places()
{
    m_lsblk.kill();
    m_lsblk.waitForFinished();
}

QString Places::homeTrash()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
}

QString Places::home() const
{
    return QDir::homePath();
}

QString Places::trash() const
{
    return homeTrash() + QStringLiteral("/files");
}

void Places::refreshDevices()
{
    // what's mounted shows now, what isn't once lsblk answers (no lsblk: only what's mounted)
    if (m_lsblk.state() == QProcess::NotRunning)
        m_lsblk.start(QStringLiteral("lsblk"), { QStringLiteral("-J"), QStringLiteral("-l"), QStringLiteral("-b"),
                                                 QStringLiteral("-o"), QStringLiteral("PATH,LABEL,FSTYPE,MOUNTPOINT,HOTPLUG,RM,SIZE") });
    showDevices();
}

void Places::showDevices()
{
    QVariantList devices = { entry(QStringLiteral("System"), QStringLiteral("/"), QStringLiteral("drive")) };
    devices.append(volumes());
    devices.append(m_unmounted);
    if (devices != m_devices) {
        m_devices = devices;
        emit devicesChanged();
    }
}

void Places::mount(const QString &device)
{
    udiskie(QStringLiteral("udiskie-mount"), device, [this, device] {
        const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
        for (const QStorageInfo &storage : mounted) {
            if (QString::fromLocal8Bit(storage.device()) == device) {
                emit this->mounted(storage.rootPath());
                return;
            }
        }
    });
}

void Places::unmount(const QString &device)
{
    udiskie(QStringLiteral("udiskie-umount"), device, [] {});
}

// runs udiskie-mount or udiskie-umount on `device`; `done` once it worked, failed() with its reason if not
void Places::udiskie(const QString &tool, const QString &device, const std::function<void()> &done)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, tool, done](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        refreshDevices();
        if (status == QProcess::NormalExit && code == 0) {
            done();
            return;
        }
        // "failed to remove /dev/sdb1: … target is busy"
        const QString output = QString::fromLocal8Bit(process->readAll()).trimmed().section(u'\n', -1);
        const bool mounting = tool == QLatin1String("udiskie-mount");
        if (output.contains(QLatin1String("busy")))
            emit failed(QStringLiteral("The drive is in use: close what's open on it, then unmount it again"));
        else
            emit failed(QStringLiteral("Couldn't %1 it: %2").arg(mounting ? QStringLiteral("mount") : QStringLiteral("unmount"),
                                                             output.contains(QLatin1String(": ")) ? output.section(QStringLiteral(": "), 1).trimmed()
                                                                                                  : QStringLiteral("%1 failed").arg(tool)));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, tool](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // the others end in finished()
        process->deleteLater();
        emit failed(QStringLiteral("%1 isn't installed (pacman -S udiskie)").arg(tool));
    });
    process->start(tool, { device });
}

QVariantList Places::crumbs(const QString &path) const
{
    // the roots a path can start from; Documents and the like are just folders in Home
    QVariantList roots = { entry(QStringLiteral("System"), QStringLiteral("/"), {}),
                           entry(QStringLiteral("Home"), home(), {}),
                           entry(QStringLiteral("Trash"), trash(), {}) };
    roots.append(volumes());

    QVariantMap root;
    for (const QVariant &candidate : std::as_const(roots)) {
        const QVariantMap map = candidate.toMap();
        const QString rootPath = map.value(QStringLiteral("path")).toString();
        if (holds(rootPath, path) && rootPath.size() > root.value(QStringLiteral("path")).toString().size())
            root = map;
    }

    QVariantList crumbs;
    QString at = root.value(QStringLiteral("path")).toString();
    crumbs.append(QVariantMap { { QStringLiteral("name"), root.value(QStringLiteral("name")) },
                                { QStringLiteral("path"), at } });
    const QStringList rest = path.mid(at.size()).split(u'/', Qt::SkipEmptyParts);
    for (const QString &name : rest) {
        at = (at == QLatin1String("/") ? QString() : at) + u'/' + name;
        crumbs.append(QVariantMap { { QStringLiteral("name"), name }, { QStringLiteral("path"), at } });
    }
    return crumbs;
}

QString Places::placeFor(const QString &path) const
{
    QString best;
    for (const QVariantList *list : { &m_places, &m_bookmarks, &m_devices }) {
        for (const QVariant &candidate : *list) {
            const QString placePath = candidate.toMap().value(QStringLiteral("path")).toString();
            if (holds(placePath, path) && placePath.size() > best.size())
                best = placePath;
        }
    }
    return best;
}

bool Places::isTrashFolder(const QString &path)
{
    // the home trash, or a drive's $topdir/.Trash-$uid or $topdir/.Trash/$uid
    static const QRegularExpression driveTrash(QStringLiteral("/(\\.Trash-\\d+|\\.Trash/\\d+)/files$"));
    return path == homeTrash() + QStringLiteral("/files") || driveTrash.match(path).hasMatch();
}

bool Places::inTrashFolder(const QString &path)
{
    static const QRegularExpression driveTrash(QStringLiteral("/(\\.Trash-\\d+|\\.Trash/\\d+)/files(/|$)"));
    return holds(homeTrash() + QStringLiteral("/files"), path) || driveTrash.match(path).hasMatch();
}
