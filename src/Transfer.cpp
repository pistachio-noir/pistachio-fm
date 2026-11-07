#include "Transfer.h"

#include "Format.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Transfer {
namespace {

constexpr qsizetype Chunk = 1 << 20;

QString parentOf(const QString &path)
{
    const qsizetype at = path.lastIndexOf(u'/');
    return at <= 0 ? QStringLiteral("/") : path.left(at);
}

QString nameOf(const QString &path)
{
    return path.mid(path.lastIndexOf(u'/') + 1);
}

QString reasonFor(int error)
{
    switch (error) {
    case ENOSPC:
    case EDQUOT:
        return QStringLiteral("the drive is full");
    case EACCES:
    case EPERM:
        return QStringLiteral("no permission");
    case EROFS:
        return QStringLiteral("the drive is read-only");
    case ENOENT:
        return QStringLiteral("it isn't there any more");
    case ENAMETOOLONG:
        return QStringLiteral("the name is too long for that drive");
    default:
        return QString::fromLocal8Bit(std::strerror(error)).toLower();
    }
}

// rename(2) that never replaces anything; a check first where the drive can't promise that
int renameNoReplace(const QString &from, const QString &to)
{
    const QByteArray source = QFile::encodeName(from);
    const QByteArray target = QFile::encodeName(to);
    if (::renameat2(AT_FDCWD, source.constData(), AT_FDCWD, target.constData(), RENAME_NOREPLACE) == 0)
        return 0;
    if (errno != EINVAL && errno != ENOSYS)
        return errno;
    if (present(to))
        return EEXIST;
    return ::rename(source.constData(), target.constData()) == 0 ? 0 : errno;
}

// the folder `source` itself or one inside it (links are copied as links, so they never count)
bool insideItself(const QString &source, const QString &dir)
{
    const QFileInfo info(source);
    if (info.isSymLink() || !info.isDir())
        return false;
    const QString folder = info.canonicalFilePath();
    const QString into = QFileInfo(dir).canonicalFilePath();
    return !folder.isEmpty() && (into == folder || into.startsWith(folder + u'/'));
}

class Copier
{
public:
    Copier(const std::atomic<bool> &stop, const std::function<void(const Progress &)> &report)
        : m_stop(stop)
        , m_report(report)
    {
        m_sinceReport.start();
    }

    Progress progress;
    QString reason;

    bool copy(const QString &from, const QString &to)
    {
        if (m_stop)
            return false;
        const QByteArray source = QFile::encodeName(from);
        const QByteArray target = QFile::encodeName(to);
        struct stat st;
        if (::lstat(source.constData(), &st) != 0)
            return fail(errno);

        if (S_ISLNK(st.st_mode)) {
            QByteArray link(st.st_size > 0 ? st.st_size : PATH_MAX, Qt::Uninitialized);
            const ssize_t length = ::readlink(source.constData(), link.data(), link.size());
            if (length < 0)
                return fail(errno);
            link.truncate(length);
            if (::symlink(link.constData(), target.constData()) != 0)
                return fail(errno);
            ++progress.files;
            tick();
            return true;
        }

        if (S_ISDIR(st.st_mode)) {
            if (::access(source.constData(), R_OK | X_OK) != 0)
                return fail(errno);
            if (::mkdir(target.constData(), 0700) != 0)
                return fail(errno);
            const QStringList names = QDir(from).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            for (const QString &name : names) {
                if (!copy(from + u'/' + name, to + u'/' + name))
                    return false;
            }
            // last, so a read-only folder could still be filled
            ::chmod(target.constData(), st.st_mode & 07777);
            const struct timespec times[2] = { st.st_atim, st.st_mtim };
            ::utimensat(AT_FDCWD, target.constData(), times, AT_SYMLINK_NOFOLLOW);
            return true;
        }

        if (!S_ISREG(st.st_mode)) {
            reason = QStringLiteral("it's a special file");
            return false;
        }
        return copyFile(source, target, st);
    }

private:
    bool copyFile(const QByteArray &source, const QByteArray &target, const struct stat &st)
    {
        const int in = ::open(source.constData(), O_RDONLY | O_CLOEXEC);
        if (in < 0)
            return fail(errno);
        const int out = ::open(target.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (out < 0) {
            const int error = errno;
            ::close(in);
            return fail(error);
        }

        // copy_file_range lets the drive do it (instantly where it can share the blocks); plain
        // read and write where it can't
        bool kernelCopy = true;
        QByteArray buffer;
        int error = 0;
        for (;;) {
            if (m_stop) {
                error = ECANCELED;
                break;
            }
            ssize_t copied;
            if (kernelCopy) {
                copied = ::copy_file_range(in, nullptr, out, nullptr, Chunk, 0);
                if (copied < 0 && (errno == EXDEV || errno == ENOSYS || errno == EINVAL || errno == EOPNOTSUPP)) {
                    kernelCopy = false;
                    continue;
                }
            } else {
                if (buffer.isEmpty())
                    buffer.resize(Chunk);
                copied = ::read(in, buffer.data(), Chunk);
                for (ssize_t written = 0; copied > 0 && written < copied;) {
                    const ssize_t n = ::write(out, buffer.constData() + written, copied - written);
                    if (n < 0) {
                        copied = -1;
                        break;
                    }
                    written += n;
                }
            }
            if (copied < 0) {
                if (errno == EINTR)
                    continue;
                error = errno;
                break;
            }
            if (copied == 0)
                break;
            progress.bytes += copied;
            tick();
        }

        if (error == 0) {
            ::fchmod(out, st.st_mode & 07777);
            const struct timespec times[2] = { st.st_atim, st.st_mtim };
            ::futimens(out, times);
        }
        ::close(in);
        if (::close(out) != 0 && error == 0)
            error = errno;
        if (error != 0) {
            ::unlink(target.constData());
            return error == ECANCELED ? false : fail(error);
        }
        ++progress.files;
        tick();
        return true;
    }

    bool fail(int error)
    {
        if (reason.isEmpty())
            reason = reasonFor(error);
        return false;
    }

    void tick()
    {
        if (m_sinceReport.elapsed() < 100)
            return;
        m_report(progress);
        m_sinceReport.restart();
    }

    const std::atomic<bool> &m_stop;
    const std::function<void(const Progress &)> &m_report;
    QElapsedTimer m_sinceReport;
};

} // namespace

void survey(const QString &path, Progress &progress)
{
    struct stat st;
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0)
        return;
    if (S_ISDIR(st.st_mode)) {
        const QStringList names = QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString &name : names)
            survey(path + u'/' + name, progress);
        return;
    }
    ++progress.totalFiles;
    if (S_ISREG(st.st_mode))
        progress.totalBytes += st.st_size;
}

QString join(const QString &dir, const QString &name)
{
    return (dir == QLatin1String("/") ? QString() : dir) + u'/' + name;
}

bool present(const QString &path)
{
    struct stat st;
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0;
}

QString uniqueName(const QString &dir, const QString &name)
{
    if (!present(join(dir, name)))
        return name;
    const qsizetype dot = name.lastIndexOf(u'.');
    const QString stem = dot > 0 ? name.left(dot) : name;
    const QString extension = dot > 0 ? name.mid(dot) : QString();
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(stem).arg(n).arg(extension);
        if (!present(join(dir, candidate)))
            return candidate;
    }
}

bool removeAll(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir() && !info.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

Result run(const QList<Item> &items, bool move, const std::atomic<bool> &stop,
           const std::function<void(const Progress &)> &report)
{
    Result result;
    Copier copier(stop, report);
    QList<Item> toCopy; // for a move, the ones on another drive

    for (const Item &item : items) {
        if (stop)
            break;
        const QString dir = QDir::cleanPath(item.dir);
        if (insideItself(item.source, dir)) {
            result.intoItself.append(item.source);
            continue;
        }
        if (!move) {
            toCopy.append(item);
            continue;
        }
        if (parentOf(item.source) == dir && nameOf(item.source) == item.name) {
            result.done.append({ item.source, item.source }); // it's already here
            continue;
        }
        const QString target = join(dir, uniqueName(dir, item.name));
        const int error = renameNoReplace(item.source, target);
        if (error == 0) {
            result.done.append({ item.source, target });
        } else if (error == EXDEV) {
            toCopy.append(item);
        } else {
            result.failures.append(item.source);
            if (result.reason.isEmpty())
                result.reason = reasonFor(error);
        }
    }

    copier.progress.moving = move;
    if (toCopy.size() == 1)
        copier.progress.name = nameOf(toCopy.first().source);
    for (const Item &item : std::as_const(toCopy))
        survey(item.source, copier.progress);

    for (const Item &item : std::as_const(toCopy)) {
        if (stop)
            break;
        const QString dir = QDir::cleanPath(item.dir);
        const QString target = join(dir, uniqueName(dir, item.name));
        if (!copier.copy(item.source, target)) {
            removeAll(target); // whatever was half made
            if (stop)
                break;
            result.failures.append(item.source);
            continue;
        }
        if (move && !removeAll(item.source)) {
            // both are there now; the copy is whole, so it stays
            result.failures.append(item.source);
            if (copier.reason.isEmpty())
                copier.reason = QStringLiteral("it was copied, but the original couldn't be removed");
        }
        result.done.append({ item.source, target });
    }
    if (result.reason.isEmpty())
        result.reason = copier.reason;
    return result;
}

QString describe(const Progress &progress)
{
    const QString verb = progress.moving ? QStringLiteral("Moving") : QStringLiteral("Copying");
    const QString what = progress.name.isEmpty()
        ? QStringLiteral("%1 of %2").arg(qMin(progress.files + 1, progress.totalFiles)).arg(progress.totalFiles)
        : QStringLiteral("“%1”").arg(progress.name);
    return QStringLiteral("%1 %2 · %3 of %4").arg(verb, what, Format::size(progress.bytes), Format::size(progress.totalBytes));
}

} // namespace Transfer
