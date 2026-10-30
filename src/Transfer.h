#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <utility>

// Copying and moving for paste and undo, run on a worker thread: folders with everything in them,
// links as links (never what they point to), permissions and modification times kept. Each item
// either lands whole or leaves nothing behind.
namespace Transfer {

struct Item
{
    QString source;
    QString dir; // where it goes
    QString name; // what it's called there; "name (2)" if that's taken
};

struct Progress
{
    bool moving = false; // a move to another drive, which copies
    QString name; // the only item, when there's one
    int files = 0;
    int totalFiles = 0;
    qint64 bytes = 0;
    qint64 totalBytes = 0;
};

struct Result
{
    QList<std::pair<QString, QString>> done; // (source, where it is now); a move to where it already was keeps its path
    QStringList failures; // sources
    QStringList intoItself; // folders that would have gone inside themselves (sources)
    QString reason; // why the first failure failed, "" if unknown: "the drive is full"
};

// `report` is called on the worker's thread every 100 ms or so while there are bytes to copy;
// `stop` ends the run early (what was half copied is removed)
Result run(const QList<Item> &items, bool move, const std::atomic<bool> &stop,
           const std::function<void(const Progress &)> &report);

// adds the files under `path` (links count as files, never followed) to totalFiles and totalBytes
void survey(const QString &path, Progress &progress);

// "Copying 3 of 14 · 340 MB of 1.2 GB", "Moving “disk.iso” · 340 MB of 1.2 GB"
QString describe(const Progress &progress);

// "name (2).ext", "name (3).ext"… if `name` is taken in `dir`
QString uniqueName(const QString &dir, const QString &name);
QString join(const QString &dir, const QString &name);
bool present(const QString &path); // a broken link counts: it still takes the name
// a folder with everything in it, but never what a link inside points to
bool removeAll(const QString &path);

} // namespace Transfer
