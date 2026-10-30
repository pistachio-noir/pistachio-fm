#include "Format.h"

#include <QLocale>
#include <QMimeType>

#include <cmath>

namespace Format {

QString size(qint64 bytes)
{
    if (bytes < 1000)
        return QStringLiteral("%1 B").arg(bytes);

    static const char *const units[] = { "KB", "MB", "GB", "TB", "PB" };
    double value = bytes / 1000.0;
    int unit = 0;
    while (unit < 4 && std::round(value) >= 1000) {
        value /= 1000;
        ++unit;
    }
    if (unit > 0 && value < 9.95) {
        QString text = QString::number(value, 'f', 1);
        if (text.endsWith(QLatin1String(".0")))
            text.chop(2);
        return text + u' ' + QLatin1String(units[unit]);
    }
    return QString::number(std::round(value), 'f', 0) + u' ' + QLatin1String(units[unit]);
}

QString date(const QDateTime &when, const QDateTime &now)
{
    const QDate day = when.date();
    const QDate today = now.date();
    if (day == today)
        return QStringLiteral("Today");
    if (day == today.addDays(-1))
        return QStringLiteral("Yesterday");
    // the C locale, so month names stay English like the rest of the UI
    const QLocale c = QLocale::c();
    return c.toString(day, day.year() == today.year() ? QStringLiteral("MMM d") : QStringLiteral("MMM d, yyyy"));
}

QString items(int count)
{
    return count == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 items").arg(count);
}

QString kind(const QMimeType &mime)
{
    const QString name = mime.name();
    if (name == QLatin1String("inode/directory"))
        return QStringLiteral("folder");
    if (name.startsWith(QLatin1String("image/")))
        return QStringLiteral("image");
    if (name.startsWith(QLatin1String("audio/")))
        return QStringLiteral("audio");
    if (name.startsWith(QLatin1String("video/")))
        return QStringLiteral("video");

    // before archives: office files and epubs are zip files underneath
    static const QStringList documents = {
        QStringLiteral("application/pdf"), QStringLiteral("text/plain"), QStringLiteral("text/markdown"),
        QStringLiteral("text/csv"), QStringLiteral("application/rtf"), QStringLiteral("application/epub+zip"),
        QStringLiteral("application/msword"), QStringLiteral("application/vnd.ms-excel"),
        QStringLiteral("application/vnd.ms-powerpoint"),
    };
    if (documents.contains(name) || name.startsWith(QLatin1String("application/vnd.oasis.opendocument."))
        || name.startsWith(QLatin1String("application/vnd.openxmlformats-officedocument.")))
        return QStringLiteral("document");

    static const QStringList archives = {
        QStringLiteral("application/zip"), QStringLiteral("application/x-tar"), QStringLiteral("application/gzip"),
        QStringLiteral("application/x-7z-compressed"), QStringLiteral("application/vnd.rar"),
        QStringLiteral("application/x-rar"), QStringLiteral("application/zstd"), QStringLiteral("application/x-xz"),
        QStringLiteral("application/x-bzip2"), QStringLiteral("application/x-compressed-tar"),
        QStringLiteral("application/x-zstd-compressed-tar"), QStringLiteral("application/x-xz-compressed-tar"),
        QStringLiteral("application/x-bzip2-compressed-tar"), QStringLiteral("application/x-iso9660-image"),
        QStringLiteral("application/x-cd-image"),
    };
    for (const QString &archive : archives) {
        if (mime.inherits(archive))
            return QStringLiteral("archive");
    }

    // other text: source code, scripts, config, JSON, HTML
    if (mime.inherits(QStringLiteral("text/plain")))
        return QStringLiteral("code");
    return QStringLiteral("file");
}

} // namespace Format
