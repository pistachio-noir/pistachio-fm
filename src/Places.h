#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// The sidebar's places and devices, and how a path reads as breadcrumbs. Each entry is
// { name, path, icon }, `icon` naming a glyph in Icons.qml.
class Places : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString home READ home CONSTANT)
    Q_PROPERTY(QString trash READ trash CONSTANT) // the Trash place: the home trash's files/ folder
    Q_PROPERTY(QVariantList places READ places CONSTANT)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // the folders pinned in ~/.config/gtk-3.0/bookmarks, the file Nautilus keeps them in (so they're shared)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)

public:
    explicit Places(QObject *parent = nullptr);
    ~Places() override; // stops lsblk, if it's still answering

    // ~/.local/share/Trash (or $XDG_DATA_HOME/Trash), as QFile::moveToTrash uses it
    static QString homeTrash();

    QString home() const;
    QString trash() const;
    QVariantList places() const { return m_places; }
    QVariantList devices() const { return m_devices; }
    QVariantList bookmarks() const { return m_bookmarks; }

    static QString bookmarksFile();
    // a folder at the end of the bookmarks (false, with failed(), for anything else); other lines are kept as they are
    Q_INVOKABLE bool addBookmark(const QString &path);
    Q_INVOKABLE void addBookmarks(const QList<QUrl> &urls); // dropped on the sidebar
    Q_INVOKABLE void removeBookmark(const QString &path);
    Q_INVOKABLE bool isBookmarked(const QString &path) const;

    // [{ name, path }] from the place that holds `path` (Home, Trash, a drive, or System) down to it
    Q_INVOKABLE QVariantList crumbs(const QString &path) const;
    // the sidebar entry to light up for `path`: the place or device holding it most closely
    Q_INVOKABLE QString placeFor(const QString &path) const;
    // a trash's own files/ folder (its entries can be restored), not a folder inside it
    // a removable drive's partition, through udiskie (udisks asks polkit, so the shell's agent asks for a
    // password if one is needed); mounted() says where it went
    Q_INVOKABLE void mount(const QString &device);
    Q_INVOKABLE void unmount(const QString &device);

    Q_INVOKABLE bool isTrash(const QString &path) const { return isTrashFolder(path); }
    Q_INVOKABLE bool inTrash(const QString &path) const { return inTrashFolder(path); }
    static bool isTrashFolder(const QString &path);
    static bool inTrashFolder(const QString &path);

signals:
    void devicesChanged();
    void bookmarksChanged();
    void mounted(const QString &path);
    void failed(const QString &message);

private:
    void refreshDevices();
    void readBookmarks();
    bool writeBookmarks(const QList<QByteArray> &lines);
    void showDevices();
    void udiskie(const QString &tool, const QString &device, const std::function<void()> &done);

    QVariantList m_places;
    QVariantList m_devices;
    QVariantList m_bookmarks;
    QVariantList m_unmounted; // removable drives' partitions, from lsblk
    QProcess m_lsblk;
    QTimer m_poll;
};
