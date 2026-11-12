#pragma once

#include <QAbstractListModel>
#include <QCollator>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

struct FileEntry
{
    QString name;
    QString path;
    QString kind; // see Format::kind
    QString suffix; // lower case, for sorting by type
    qint64 size = 0;
    QDateTime modified;
    int itemCount = Counting; // folders only
    bool isDir = false;
    bool hidden = false;
    bool picture = false; // one Qt can show (its thumbnail)

    enum : int { Counting = -1, Unreadable = -2 };
};

// One folder's entries for the grid and list views: listed off the GUI thread, folders first, kept
// up to date while the folder changes on disk, with the selection, search filter, sort order and
// the back and forward history.
class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool recursive READ recursive WRITE setRecursive NOTIFY recursiveChanged)
    Q_PROPERTY(SortKey sortKey READ sortKey WRITE setSortKey NOTIFY sortKeyChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged) // before the search filter
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged) // why the folder can't be shown, or ""
    Q_PROPERTY(qint64 freeBytes READ freeBytes NOTIFY freeBytesChanged)
    Q_PROPERTY(int currentRow READ currentRow NOTIFY currentRowChanged) // keyboard focus, -1 for none
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(qint64 selectedSize READ selectedSize NOTIFY selectionChanged) // files only
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY selectionChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY pathChanged)
    // the status line: "12 items · 212 GB free", "12 items · 1 selected · 2.4 MB", "3 of 12 items · …"
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)

public:
    // date and size sort newest and biggest first; folders always come first, by name
    enum SortKey { ByName, ByModified, BySize, ByType };
    Q_ENUM(SortKey)

    // Replace: just this row; Toggle: add or drop it (Ctrl+click); Extend: from the anchor (Shift)
    enum SelectMode { Replace, Toggle, Extend };
    Q_ENUM(SelectMode)

    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        KindRole,
        IsDirRole,
        SizeRole,
        ModifiedRole,
        SizeTextRole, // "2.4 MB", or "14 items" for a folder
        ModifiedTextRole, // "Oct 27"
        DetailRole, // the grid tile's second line: "2.4 MB · Oct 27", or "14 items"
        SelectedRole,
        HiddenRole,
        PictureRole,
    };

    explicit FolderModel(QObject *parent = nullptr);
    ~FolderModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return m_path; }
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    bool recursive() const { return m_recursive; }
    void setRecursive(bool recursive);
    SortKey sortKey() const { return m_sortKey; }
    void setSortKey(SortKey key);
    int count() const { return int(m_rows.size()); }
    int totalCount() const { return m_total; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    qint64 freeBytes() const { return m_freeBytes; }
    int currentRow() const { return m_rowOf.value(m_current, -1); }
    int selectedCount() const { return int(m_selected.size()); }
    qint64 selectedSize() const;
    QStringList selectedPaths() const;
    bool canGoBack() const { return !m_back.isEmpty(); }
    bool canGoForward() const { return !m_forward.isEmpty(); }
    bool canGoUp() const { return !m_path.isEmpty() && m_path != QLatin1String("/"); }
    QString summary() const;

    // Opens a folder (a path, "~/…", a file:// URL, or relative to this folder) and adds it to the
    // history. A file opens its folder with the file selected. False, with failed(), if it's missing.
    Q_INVOKABLE bool go(const QString &where);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void up();
    Q_INVOKABLE void reload();

    Q_INVOKABLE void select(int row, FolderModel::SelectMode mode = Replace);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection(); // and the keyboard position
    // selects these paths, now or, for any not listed yet (just renamed or made), after the next listing
    Q_INVOKABLE void selectPaths(const QStringList &paths);
    Q_INVOKABLE int rowOf(const QString &path) const { return m_rowOf.value(path, -1); }
    Q_INVOKABLE QVariantMap get(int row) const; // name, path, kind, isDir

signals:
    void pathChanged();
    void showHiddenChanged();
    void filterChanged();
    void recursiveChanged();
    void sortKeyChanged();
    void countChanged();
    void loadingChanged();
    void errorChanged();
    void freeBytesChanged();
    void currentRowChanged();
    void selectionChanged();
    void historyChanged();
    void summaryChanged();
    void loaded(); // a listing landed (the folder's item counts may still be on their way)
    void failed(const QString &message);

private:
    void load(const QString &path);
    void startListing();
    void listed(QList<FileEntry> entries, const QString &error);
    void counted(const QHash<QString, int> &counts);
    void rebuild();
    void applyRows(QList<FileEntry> next);
    void reindex();
    void setSelection(const QSet<QString> &next);
    void setCurrent(const QString &path);
    void setLoading(bool loading);
    void setError(const QString &error);
    bool lessThan(const FileEntry &a, const FileEntry &b) const;
    QString resolve(const QString &where) const;

    QString m_path;
    QStringList m_back;
    QStringList m_forward;
    QList<FileEntry> m_all; // everything in the folder, hidden files too
    QHash<QString, qsizetype> m_allIndex;
    QList<FileEntry> m_rows; // what the views show, sorted
    QHash<QString, int> m_rowOf;
    QSet<QString> m_selected;
    QStringList m_pendingSelection;
    QElapsedTimer m_pendingSince; // a pending selection lapses after a few seconds
    QString m_anchor;
    QString m_current;
    QString m_filter;
    QString m_error;
    SortKey m_sortKey = ByName;
    bool m_showHidden = false;
    bool m_recursive = false;
    bool m_loading = false;
    int m_total = 0;
    qint64 m_freeBytes = 0;
    QCollator m_collator;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    // bumped on every listing; a worker whose number is stale stops and its results are dropped
    std::shared_ptr<std::atomic<quint64>> m_generation = std::make_shared<std::atomic<quint64>>(0);
};
