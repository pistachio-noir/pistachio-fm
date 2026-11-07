#pragma once

#include "Transfer.h"

#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <functional>
#include <memory>

class QJSEngine;
class QQmlEngine;

// What the menus, dialogs and keys do to files. Quick operations return "" or an error to show in
// the dialog; slow ones (pasting, compressing, deleting for good) run in the background and report
// with finished(). Anything that fails also sends failed() with a line for the status bar.
//
// Copy and cut share the desktop's clipboard (the formats Nautilus and Dolphin use), so files go
// both ways between them and this app. Most changes can be undone, the latest first.
class FileOps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(QStringList cutPaths READ cutPaths NOTIFY clipboardChanged) // faded until they're pasted
    // "Copying 3 of 14 · 340 MB of 1.2 GB" while a paste takes a while, else ""
    Q_PROPERTY(QString progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)

public:
    explicit FileOps(QObject *parent = nullptr);
    ~FileOps() override; // stops a paste that's still going, removing its half-copied item

    bool busy() const { return m_running > 0; }
    bool canPaste() const { return !m_clipboard.isEmpty(); }
    QStringList cutPaths() const { return m_clipboardCut ? m_clipboard : QStringList(); }
    QString progress() const { return m_progress; }
    bool canUndo() const { return !m_undo.isEmpty(); }

    // why `name` can't be used for a new entry in `dir` ("" if it can); `except` is the entry being renamed
    Q_INVOKABLE QString checkName(const QString &dir, const QString &name, const QString &except = {}) const;
    Q_INVOKABLE QString rename(const QString &path, const QString &name);
    Q_INVOKABLE QString makeFolder(const QString &dir, const QString &name);
    Q_INVOKABLE QString makeFile(const QString &dir, const QString &name); // empty, any name or extension

    // the freedesktop.org trash: ~/.local/share/Trash, or the drive's own .Trash-UID
    Q_INVOKABLE bool trash(const QStringList &paths);
    Q_INVOKABLE bool restore(const QStringList &trashedPaths); // back where each was trashed from
    Q_INVOKABLE void deleteForever(const QStringList &paths);
    Q_INVOKABLE void emptyTrash(const QString &files); // that trash's files/ folder: the home one or a drive's

    Q_INVOKABLE void copy(const QStringList &paths);
    Q_INVOKABLE void cut(const QStringList &paths);
    // what was copied or cut, into `dir`; a name that's taken there keeps both ("name (2).ext")
    Q_INVOKABLE void paste(const QString &dir);
    // files dragged here from this window or another app: a move on the same drive, a copy to another
    // (as other file managers do); Ctrl copies, Shift moves. Onto the Trash place, they're trashed.
    Q_INVOKABLE void drop(const QList<QUrl> &urls, const QString &dir);
    Q_INVOKABLE void copyPaths(const QStringList &paths); // as text
    // a .zip of these, next to them in `dir`, through bsdtar (libarchive)
    Q_INVOKABLE void compress(const QStringList &paths, const QString &dir);
    // into a new folder named after it, in `dir`
    Q_INVOKABLE void extract(const QString &archive, const QString &dir);

    // takes back the latest paste, rename, new folder or file, move to trash, restore, compress or extract
    Q_INVOKABLE void undo();
    // stops the paste or drop under way and drops the ones waiting; what's done stays (and can be undone)
    Q_INVOKABLE void stop();

    // what Properties shows, as text: { name, kind, type, size, location, deletedFrom, deleted, created,
    // modified, permissions, link, measure }; "" for what's unknown or doesn't apply (a trashed item has
    // deletedFrom and deleted instead of a location). A folder's size (measure: true) comes from measure().
    Q_INVOKABLE QVariantMap properties(const QString &path) const;
    // adds up a folder's files in the background, for measured()
    Q_INVOKABLE void measure(const QString &path);

    // "~/Documents/thesis-draft.pdf"
    Q_INVOKABLE QString displayPath(const QString &path) const;

signals:
    void busyChanged();
    void clipboardChanged();
    void progressChanged();
    void undoChanged();
    // a background job is done: "paste" (a drop too), "undo" or "compress" with the paths to select, or "delete"
    void finished(const QString &job, const QStringList &paths);
    void failed(const QString &message);
    void notice(const QString &message); // "Undid rename", for the status bar
    void measured(const QString &path, const QString &size); // "340 MB · 1,234 files"

private:
    // what undo takes back
    struct Step
    {
        enum Kind { Copied, Moved, Renamed, MadeFolder, MadeFile, Trashed, Restored, Compressed, Extracted } kind;
        QList<std::pair<QString, QString>> moves; // Moved, Renamed, Trashed: (where it was, where it went)
        QStringList made; // Copied, MadeFolder, MadeFile, Restored, Compressed, Extracted: what to take away
    };

    void remember(const Step &step);
    void put(const QStringList &paths, const QString &dir, bool move); // paste and drop
    QStringList trashAll(const QStringList &paths, QList<std::pair<QString, QString>> *trashed = nullptr);
    QStringList restoreAll(const QStringList &trashedPaths); // where they went back to
    void setClipboard(const QStringList &paths, bool cut);
    void readClipboard();
    void transfer(const QList<Transfer::Item> &items, bool move, const std::function<void(const Transfer::Result &)> &then);
    void setProgress(const QString &text);
    void bsdtar(const QString &program, const QStringList &args, const QString &made, Step::Kind kind);
    void started();
    void ended();

    int m_running = 0;
    QStringList m_clipboard; // local files on the clipboard
    bool m_clipboardCut = false;
    QString m_progress;
    QList<Step> m_undo;
    QThreadPool m_transfers; // one paste at a time, the next waits its turn
    std::shared_ptr<std::atomic<bool>> m_stop = std::make_shared<std::atomic<bool>>(false);
};
