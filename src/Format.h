#pragma once

#include <QDateTime>
#include <QString>

class QMimeType;

// How sizes, dates, counts and kinds read on the boards: "2.4 MB", "Oct 27", "14 items", "code".
namespace Format {

// SI units (1 KB = 1000 B): whole kilobytes, one decimal below 10 MB/GB/TB ("2.4 MB", "34 MB")
QString size(qint64 bytes);

// "Today", "Yesterday", "Oct 27", or "Oct 27, 2024" in another year
QString date(const QDateTime &when, const QDateTime &now = QDateTime::currentDateTime());

// "1 item", "14 items"
QString items(int count);

// folder, document, code, image, audio, video, archive or file: picks the badge's glyph and colour
QString kind(const QMimeType &mime);

} // namespace Format
