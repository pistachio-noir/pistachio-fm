#include "FolderModel.h"

#include "Format.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirListing>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPointer>
#include <QStorageInfo>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>

namespace {

QString parentOf(const QString &path)
{
    const qsizetype at = path.lastIndexOf(u'/');
    return at <= 0 ? QStringLiteral("/") : path.left(at);
}

// "/home/sx/" for "/home/sx", "/" for "/"
QString childPrefix(const QString &path)
{
    return path == QLatin1String("/") ? path : path + u'/';
}

FileEntry fileEntry(const QFileInfo &info)
{
    QMimeDatabase mimes;
    static const QList<QByteArray> pictureTypes = QImageReader::supportedMimeTypes();
    FileEntry entry;
    entry.name = info.fileName();
    entry.path = info.absoluteFilePath();
    entry.isDir = info.isDir();
    entry.hidden = entry.name.startsWith(u'.');
    entry.modified = info.lastModified();
    if (entry.isDir) {
        entry.size = FileEntry::Counting;
        entry.kind = QStringLiteral("folder");
    } else {
        entry.size = info.size();
        // by name only: reading every file's first bytes would make big folders slow
        const QMimeType mime = mimes.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
        entry.kind = Format::kind(mime);
        entry.picture = entry.kind == QLatin1String("image") && pictureTypes.contains(mime.name().toLatin1());
        entry.suffix = info.suffix().toLower();
    }
    return entry;
}

QList<FileEntry> listFolder(const QString &path, QString *error)
{
    QList<FileEntry> entries;
    const QFileInfo folder(path);
    if (!folder.isDir()) {
        *error = QStringLiteral("This folder isn't there any more");
        return entries;
    }
    if (!folder.isReadable() || !folder.isExecutable()) {
        *error = QStringLiteral("You don't have permission to open this folder");
        return entries;
    }

    const QFileInfoList infos = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos)
        entries.append(fileEntry(info));
    return entries;
}

int countEntries(const QString &path, bool showHidden)
{
    const QFileInfo info(path);
    if (!info.isReadable() || !info.isExecutable())
        return FileEntry::Unreadable;
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (showHidden)
        filters |= QDir::Hidden;
    return int(QDir(path).entryList(filters, QDir::NoSort).size());
}

qint64 folderSize(const QString &path, const std::shared_ptr<std::atomic<quint64>> &generation, quint64 expected)
{
    const QFileInfo folder(path);
    if (folder.isSymLink() || !folder.isReadable() || !folder.isExecutable())
        return FileEntry::Unreadable;
    qint64 bytes = 0;
    const auto flags = QDirListing::IteratorFlag::Recursive | QDirListing::IteratorFlag::IncludeHidden;
    for (const auto &item : QDirListing(path, flags)) {
        if (*generation != expected)
            return FileEntry::Counting;
        if (item.isSymLink())
            continue;
        if (item.isDir() && (!item.isReadable() || !item.isExecutable()))
            return FileEntry::Unreadable;
        if (item.isFile())
            bytes += item.size();
    }
    return bytes;
}

bool sameData(const FileEntry &a, const FileEntry &b)
{
    return a.name == b.name && a.kind == b.kind && a.size == b.size && a.modified == b.modified
        && a.itemCount == b.itemCount;
}

} // namespace

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    // a burst of changes (an unpacked archive) reloads once
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(150);
    connect(&m_reloadTimer, &QTimer::timeout, this, &FolderModel::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reloadTimer.start(); });

    for (const auto signal : { &FolderModel::countChanged, &FolderModel::selectionChanged,
                               &FolderModel::freeBytesChanged, &FolderModel::errorChanged,
                               &FolderModel::loadingChanged })
        connect(this, signal, this, &FolderModel::summaryChanged);
}

FolderModel::~FolderModel()
{
    ++*m_generation; // stops a worker that is still counting
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const FileEntry &entry = m_rows.at(index.row());

    const auto items = [&entry]() -> QString {
        if (entry.itemCount == FileEntry::Counting)
            return {};
        if (entry.itemCount == FileEntry::Unreadable)
            return QStringLiteral("Folder");
        const QString count = Format::items(entry.itemCount);
        if (entry.size == FileEntry::Counting)
            return count;
        return count + QStringLiteral(" · ") + (entry.size == FileEntry::Unreadable ? QStringLiteral("—") : Format::size(entry.size));
    };
    const auto modified = [&entry] {
        return entry.modified.isValid() ? Format::date(entry.modified) : QStringLiteral("—");
    };

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return m_recursive && !m_filter.isEmpty() ? QDir(m_path).relativeFilePath(entry.path) : entry.name;
    case PathRole:
        return entry.path;
    case KindRole:
        return entry.kind;
    case IsDirRole:
        return entry.isDir;
    case SizeRole:
        return entry.size;
    case ModifiedRole:
        return entry.modified;
    case SizeTextRole:
        return entry.isDir ? items() : Format::size(entry.size);
    case ModifiedTextRole:
        return modified();
    case DetailRole:
        return entry.isDir ? items() : Format::size(entry.size) + QStringLiteral(" · ") + modified();
    case SelectedRole:
        return m_selected.contains(entry.path);
    case HiddenRole:
        return entry.hidden;
    case PictureRole:
        return entry.picture;
    }
    return {};
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { KindRole, "kind" },
        { PictureRole, "picture" },
        { IsDirRole, "isDir" },
        { SizeRole, "size" },
        { ModifiedRole, "modified" },
        { SizeTextRole, "sizeText" },
        { ModifiedTextRole, "modifiedText" },
        { DetailRole, "detail" },
        { SelectedRole, "selected" },
        { HiddenRole, "hidden" },
    };
}

void FolderModel::setShowHidden(bool show)
{
    if (show == m_showHidden)
        return;
    m_showHidden = show;
    emit showHiddenChanged();
    rebuild();
    startListing(); // the folders' item counts change too
}

void FolderModel::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit filterChanged();
    if (m_recursive) {
        m_all.clear();
        m_allIndex.clear();
        rebuild();
        startListing();
    } else {
        rebuild();
    }
}

void FolderModel::setRecursive(bool recursive)
{
    if (recursive == m_recursive)
        return;
    m_recursive = recursive;
    emit recursiveChanged();
    m_all.clear();
    m_allIndex.clear();
    rebuild();
    startListing();
}

void FolderModel::setSortKey(SortKey key)
{
    if (key == m_sortKey)
        return;
    m_sortKey = key;
    emit sortKeyChanged();
    rebuild();
}

void FolderModel::setReverseSort(bool reverse)
{
    if (m_reverseSort == reverse)
        return;
    m_reverseSort = reverse;
    emit reverseSortChanged();
    rebuild();
}

qint64 FolderModel::selectedSize() const
{
    qint64 total = 0;
    for (const QString &path : m_selected) {
        const int row = m_rowOf.value(path, -1);
        if (row >= 0 && !m_rows.at(row).isDir)
            total += m_rows.at(row).size;
    }
    return total;
}

QStringList FolderModel::selectedPaths() const
{
    QStringList paths;
    if (m_selected.isEmpty())
        return paths;
    for (const FileEntry &entry : m_rows) {
        if (m_selected.contains(entry.path))
            paths.append(entry.path);
    }
    return paths;
}

QString FolderModel::summary() const
{
    if (!m_error.isEmpty() || (m_loading && m_all.isEmpty()))
        return {};
    QStringList parts = { m_recursive && !m_filter.isEmpty() ? (count() == 1 ? QStringLiteral("1 match")
                                                                 : QStringLiteral("%1 matches").arg(count()))
                              : m_filter.isEmpty() ? Format::items(m_total)
                                             : QStringLiteral("%1 of %2").arg(count()).arg(Format::items(m_total)) };
    if (!m_selected.isEmpty()) {
        parts.append(QStringLiteral("%1 selected").arg(m_selected.size()));
        if (const qint64 size = selectedSize(); size > 0)
            parts.append(Format::size(size));
    } else if (m_freeBytes > 0) {
        parts.append(Format::size(m_freeBytes) + QStringLiteral(" free"));
    }
    return parts.join(QStringLiteral(" · "));
}

QString FolderModel::resolve(const QString &where) const
{
    QString path = where.trimmed();
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    if (path == u'~' || path.startsWith(QLatin1String("~/")))
        path = QDir::homePath() + path.mid(1);
    if (path.isEmpty())
        return {};
    if (!path.startsWith(u'/'))
        path = childPrefix(m_path.isEmpty() ? QDir::homePath() : m_path) + path;
    return QDir::cleanPath(path);
}

QString FolderModel::completePath(const QString &input) const
{
    if (input == QLatin1String("~"))
        return QStringLiteral("~/");
    const qsizetype slash = input.lastIndexOf(u'/');
    const QString head = slash < 0 ? QString() : input.left(slash + 1);
    const QString prefix = input.mid(slash + 1);
    const QString directory = head.isEmpty() ? m_path : resolve(head);
    if (directory.isEmpty())
        return input;
    QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;
    if (prefix.startsWith(u'.'))
        filters |= QDir::Hidden;
    const QStringList names = QDir(directory).entryList(filters, QDir::Name);
    QStringList matches;
    for (const QString &name : names) {
        if (name.startsWith(prefix))
            matches.append(name);
    }
    if (matches.isEmpty())
        return input;
    QString common = matches.first();
    for (const QString &name : matches) {
        qsizetype length = 0;
        while (length < common.size() && length < name.size() && common[length] == name[length])
            ++length;
        common.truncate(length);
    }
    return head + common + (matches.size() == 1 ? QStringLiteral("/") : QString());
}

bool FolderModel::go(const QString &where)
{
    const QString path = resolve(where);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists()) {
        emit failed(QStringLiteral("Can't find “%1”").arg(where.trimmed()));
        return false;
    }
    if (!info.isDir()) {
        go(parentOf(path));
        selectPaths({ path });
        return true;
    }
    if (path == m_path) {
        reload();
        return true;
    }
    if (!m_path.isEmpty())
        m_back.append(m_path);
    m_forward.clear();
    load(path);
    emit historyChanged();
    return true;
}

void FolderModel::back()
{
    if (m_back.isEmpty())
        return;
    m_forward.append(m_path);
    load(m_back.takeLast());
    emit historyChanged();
}

void FolderModel::forward()
{
    if (m_forward.isEmpty())
        return;
    m_back.append(m_path);
    load(m_forward.takeLast());
    emit historyChanged();
}

void FolderModel::up()
{
    if (canGoUp())
        go(parentOf(m_path));
}

void FolderModel::reload()
{
    if (m_path.isEmpty())
        return;
    if (!QFileInfo(m_path).isDir()) {
        // the folder went away (deleted, unmounted): show the nearest one still there
        QString parent = parentOf(m_path);
        while (parent != QLatin1String("/") && !QFileInfo(parent).isDir())
            parent = parentOf(parent);
        go(parent);
        return;
    }
    startListing();
}

void FolderModel::load(const QString &path)
{
    const QString previous = m_path;
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_path = path;
    if (QFileInfo(path).isReadable())
        m_watcher.addPath(path);

    // coming back up from a subfolder selects the folder you were in
    m_pendingSelection.clear();
    const QString prefix = childPrefix(path);
    if (previous.startsWith(prefix) && previous.size() > prefix.size()) {
        m_pendingSelection = { prefix + previous.mid(prefix.size()).section(u'/', 0, 0) };
        m_pendingSince.start();
    }

    beginResetModel();
    m_all.clear();
    m_allIndex.clear();
    m_rows.clear();
    m_rowOf.clear();
    m_selected.clear();
    m_anchor.clear();
    m_current.clear();
    m_total = 0;
    endResetModel();

    if (!m_filter.isEmpty()) {
        m_filter.clear();
        emit filterChanged();
    }
    if (m_recursive) {
        m_recursive = false;
        emit recursiveChanged();
    }
    emit pathChanged();
    emit countChanged();
    emit selectionChanged();
    emit currentRowChanged();
    setError({});
    startListing();
}

void FolderModel::startListing()
{
    if (m_path.isEmpty())
        return;
    const quint64 generation = ++*m_generation;
    const auto latest = m_generation;
    const QString path = m_path;
    const bool showHidden = m_showHidden;
    const QString query = m_recursive ? m_filter : QString();
    const QPointer<FolderModel> self(this);
    setLoading(true);

    QThreadPool::globalInstance()->start([=] {
        // results go back through the application object, which lives on the GUI thread, where it's
        // safe to check that the model still exists
        const auto post = [&](auto apply) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [=] {
                if (self && *latest == generation)
                    apply(self.data());
            }, Qt::QueuedConnection);
        };

        QString error;
        QList<FileEntry> entries;
        if (query.isEmpty()) {
            entries = listFolder(path, &error);
        } else {
            const QFileInfo root(path);
            if (!root.isDir() || !root.isReadable() || !root.isExecutable()) {
                error = QStringLiteral("Can't search this folder");
            } else {
                QDirListing::IteratorFlags flags = QDirListing::IteratorFlag::Recursive;
                if (showHidden)
                    flags |= QDirListing::IteratorFlag::IncludeHidden;
                // Do not follow directory links: they can escape the root or loop back into it.
                for (const auto &item : QDirListing(path, flags)) {
                    if (*latest != generation)
                        return;
                    if (item.fileName().contains(query, Qt::CaseInsensitive)) {
                        FileEntry entry = fileEntry(item.fileInfo());
                        entries.append(entry);
                    }
                }
            }
        }
        if (*latest != generation)
            return;
        post([entries, error](FolderModel *model) { model->listed(entries, error); });

        // then how many items each folder holds, in batches so a slow disk shows some early
        QHash<QString, int> counts;
        QElapsedTimer sincePost;
        sincePost.start();
        for (const FileEntry &entry : entries) {
            if (!entry.isDir)
                continue;
            if (*latest != generation)
                return;
            counts.insert(entry.path, countEntries(entry.path, showHidden));
            if (sincePost.elapsed() > 100) {
                post([counts](FolderModel *model) { model->counted(counts); });
                counts.clear();
                sincePost.restart();
            }
        }
        if (!counts.isEmpty())
            post([counts](FolderModel *model) { model->counted(counts); });

        // Size scans are separate so every count appears before a large subtree is traversed.
        for (const FileEntry &entry : entries) {
            if (!entry.isDir || (entry.hidden && !showHidden))
                continue;
            if (*latest != generation)
                return;
            const qint64 size = folderSize(entry.path, latest, generation);
            post([path = entry.path, size](FolderModel *model) { model->sized(path, size); });
        }
    });
}

void FolderModel::listed(QList<FileEntry> entries, const QString &error)
{
    // a reload keeps the counts already known, so folders don't blink empty while recounted
    for (FileEntry &entry : entries) {
        const qsizetype old = m_allIndex.value(entry.path, -1);
        if (entry.isDir && old >= 0 && m_all.at(old).isDir) {
            entry.itemCount = m_all.at(old).itemCount;
            entry.size = m_all.at(old).size;
        }
    }
    m_all = std::move(entries);
    m_allIndex.clear();
    for (qsizetype i = 0; i < m_all.size(); ++i)
        m_allIndex.insert(m_all.at(i).path, i);

    QStringList oldWatches = m_watcher.directories();
    oldWatches.removeAll(m_path);
    if (!oldWatches.isEmpty())
        m_watcher.removePaths(oldWatches);
    if (m_recursive && !m_filter.isEmpty()) {
        // shortcut: watch result parents; new matches elsewhere need Reload until full recursive watching is needed.
        QSet<QString> parents;
        for (const FileEntry &entry : std::as_const(m_all))
            parents.insert(parentOf(entry.path));
        parents.remove(m_path);
        for (const QString &parent : std::as_const(parents)) {
            if (QFileInfo(parent).isDir())
                m_watcher.addPath(parent);
        }
    }

    setError(error);
    setLoading(false);
    const qint64 free = QStorageInfo(m_path).bytesAvailable();
    if (free != m_freeBytes) {
        m_freeBytes = free;
        emit freeBytesChanged();
    }
    rebuild();
    if (!m_pendingSelection.isEmpty()) {
        const QStringList pending = std::exchange(m_pendingSelection, {});
        if (m_pendingSince.isValid() && m_pendingSince.elapsed() < 3000)
            selectPaths(pending);
    }
    emit loaded();
}

void FolderModel::counted(const QHash<QString, int> &counts)
{
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const qsizetype at = m_allIndex.value(it.key(), -1);
        if (at >= 0)
            m_all[at].itemCount = it.value();
        const int row = m_rowOf.value(it.key(), -1);
        if (row >= 0 && m_rows.at(row).itemCount != it.value()) {
            m_rows[row].itemCount = it.value();
            emit dataChanged(index(row), index(row), { SizeTextRole, DetailRole });
        }
    }
}

void FolderModel::sized(const QString &path, qint64 size)
{
    const qsizetype at = m_allIndex.value(path, -1);
    if (at >= 0)
        m_all[at].size = size;
    const int row = m_rowOf.value(path, -1);
    if (row >= 0 && m_rows.at(row).size != size) {
        m_rows[row].size = size;
        emit dataChanged(index(row), index(row), { SizeRole, SizeTextRole, DetailRole });
    }
}

bool FolderModel::lessThan(const FileEntry &a, const FileEntry &b) const
{
    if (a.isDir != b.isDir)
        return a.isDir;
    switch (m_sortKey) {
    case ByModified:
        if (a.modified != b.modified)
            return a.modified > b.modified;
        break;
    case BySize:
        if (!a.isDir && a.size != b.size)
            return a.size > b.size;
        break;
    case ByType:
        if (a.kind != b.kind)
            return a.kind < b.kind;
        if (a.suffix != b.suffix)
            return a.suffix < b.suffix;
        break;
    case ByName:
        break;
    }
    const int byName = m_collator.compare(a.name, b.name);
    return byName != 0 ? byName < 0 : a.name < b.name;
}

void FolderModel::rebuild()
{
    QList<FileEntry> rows;
    int total = 0;
    for (const FileEntry &entry : std::as_const(m_all)) {
        if (entry.hidden && !m_showHidden)
            continue;
        ++total;
        if (!m_filter.isEmpty() && !entry.name.contains(m_filter, Qt::CaseInsensitive))
            continue;
        rows.append(entry);
    }
    std::stable_sort(rows.begin(), rows.end(), [this](const FileEntry &a, const FileEntry &b) {
        if (a.isDir != b.isDir)
            return a.isDir;
        return m_reverseSort ? lessThan(b, a) : lessThan(a, b);
    });
    applyRows(std::move(rows));
    m_total = total;
    emit countChanged();

    // what's no longer shown can't stay selected
    QSet<QString> kept;
    for (const QString &path : std::as_const(m_selected)) {
        if (m_rowOf.contains(path))
            kept.insert(path);
    }
    if (kept.size() != m_selected.size()) {
        m_selected = kept;
        emit selectionChanged();
    }
    emit currentRowChanged();
}

// A small change to the same rows (a file added, renamed, grown) moves and updates rows in place,
// so the views keep their scroll position; anything bigger, or a new order, resets the model.
void FolderModel::applyRows(QList<FileEntry> next)
{
    QSet<QString> nextPaths;
    for (const FileEntry &entry : next)
        nextPaths.insert(entry.path);
    QSet<QString> oldPaths;
    for (const FileEntry &entry : m_rows)
        oldPaths.insert(entry.path);

    qsizetype changes = 0;
    for (const FileEntry &entry : m_rows)
        changes += !nextPaths.contains(entry.path);
    for (const FileEntry &entry : next)
        changes += !oldPaths.contains(entry.path);

    // do the rows both lists share come in the same order?
    bool sameOrder = true;
    qsizetype i = 0;
    for (const FileEntry &entry : next) {
        if (!oldPaths.contains(entry.path))
            continue;
        while (i < m_rows.size() && !nextPaths.contains(m_rows.at(i).path))
            ++i;
        if (i >= m_rows.size() || m_rows.at(i).path != entry.path) {
            sameOrder = false;
            break;
        }
        ++i;
    }

    if (m_rows.isEmpty() || !sameOrder || changes > 64) {
        beginResetModel();
        m_rows = std::move(next);
        reindex();
        endResetModel();
        return;
    }

    for (qsizetype row = m_rows.size() - 1; row >= 0; --row) {
        if (!nextPaths.contains(m_rows.at(row).path)) {
            beginRemoveRows({}, int(row), int(row));
            m_rows.removeAt(row);
            endRemoveRows();
        }
    }
    for (qsizetype row = 0; row < next.size(); ++row) {
        if (row < m_rows.size() && m_rows.at(row).path == next.at(row).path) {
            if (!sameData(m_rows.at(row), next.at(row))) {
                m_rows[row] = next.at(row);
                emit dataChanged(index(int(row)), index(int(row)));
            }
        } else {
            beginInsertRows({}, int(row), int(row));
            m_rows.insert(row, next.at(row));
            endInsertRows();
        }
    }
    reindex();
}

void FolderModel::reindex()
{
    m_rowOf.clear();
    m_rowOf.reserve(m_rows.size());
    for (qsizetype row = 0; row < m_rows.size(); ++row)
        m_rowOf.insert(m_rows.at(row).path, int(row));
}

void FolderModel::select(int row, SelectMode mode)
{
    if (row < 0 || row >= m_rows.size())
        return;
    const QString path = m_rows.at(row).path;
    QSet<QString> next;
    switch (mode) {
    case Replace:
        next.insert(path);
        m_anchor = path;
        break;
    case Toggle:
        next = m_selected;
        if (!next.remove(path))
            next.insert(path);
        m_anchor = path;
        break;
    case Extend: {
        const int from = m_rowOf.value(m_anchor, row);
        const auto [low, high] = std::minmax(from, row);
        for (int i = low; i <= high; ++i)
            next.insert(m_rows.at(i).path);
        if (m_anchor.isEmpty())
            m_anchor = path;
        break;
    }
    }
    setCurrent(path);
    setSelection(next);
}

void FolderModel::selectAll()
{
    QSet<QString> next;
    for (const FileEntry &entry : std::as_const(m_rows))
        next.insert(entry.path);
    setSelection(next);
}

void FolderModel::clearSelection()
{
    // the keyboard starts over from the first item too
    m_anchor.clear();
    setCurrent({});
    setSelection({});
}

void FolderModel::selectPaths(const QStringList &paths)
{
    QSet<QString> next;
    bool missing = false;
    for (const QString &path : paths) {
        if (m_rowOf.contains(path))
            next.insert(path);
        else
            missing = true;
    }
    if (missing) {
        m_pendingSelection = paths;
        m_pendingSince.start();
    }
    if (!next.isEmpty() || !missing) {
        m_anchor = paths.value(0);
        setCurrent(paths.value(0));
        setSelection(next);
    }
}

QVariantMap FolderModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const FileEntry &entry = m_rows.at(row);
    return {
        { QStringLiteral("name"), entry.name },
        { QStringLiteral("path"), entry.path },
        { QStringLiteral("kind"), entry.kind },
        { QStringLiteral("isDir"), entry.isDir },
    };
}

void FolderModel::setSelection(const QSet<QString> &next)
{
    if (next == m_selected)
        return;
    const QSet<QString> previous = std::exchange(m_selected, next);
    for (const QSet<QString> &side : { previous - next, next - previous }) {
        for (const QString &path : side) {
            const int row = m_rowOf.value(path, -1);
            if (row >= 0)
                emit dataChanged(index(row), index(row), { SelectedRole });
        }
    }
    emit selectionChanged();
}

void FolderModel::setCurrent(const QString &path)
{
    if (path == m_current)
        return;
    m_current = path;
    emit currentRowChanged();
}

void FolderModel::setLoading(bool loading)
{
    if (loading == m_loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void FolderModel::setError(const QString &error)
{
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
