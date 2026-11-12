#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// The apps that open files: the installed desktop entries (~/.local/share/applications first, then the
// system's), which of them open a file's type, the desktop's default for a type (mimeapps.list, read
// in xdg-open's order and written where xdg-open and the shell follow it), and starting one on a file.
// Their icons come from "image://appicon/<icon>", the icon theme Qt finds (hicolor here).
class Apps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // no default: QML takes a default-constructible singleton's constructor over create(), and the providers go missing
    explicit Apps(QObject *parent);
    static Apps *create(QQmlEngine *engine, QJSEngine *); // adds the app icon and thumbnail providers to the engine

    // { mime, type: "Markdown document", apps: [{ id, name, icon, isDefault, terminal }] }: the default first, then the
    // apps for the type itself, then for what it's a kind of (text editors for Markdown), each by name.
    // `icon` is "" where the theme has none.
    Q_INVOKABLE QVariantMap forFile(const QString &path) const;
    Q_INVOKABLE QVariantMap forMime(const QString &type) const; // app choices without a sample file
    Q_INVOKABLE QVariantList all() const; // every app a launcher shows, by name
    Q_INVOKABLE QString defaultName(const QString &path) const; // "Loupe", or "" for none
    // starts app `id` on `path` (a terminal app in $TERMINAL, or kitty); with `remember`, it becomes the
    // desktop's default for the file's type
    Q_INVOKABLE bool open(const QString &id, const QString &path, bool remember);
    // each file with its type's default (a double-click, Enter). FM starts the app itself: Qt's openUrl
    // goes through the desktop portal, which asks again whatever mimeapps.list says.
    Q_INVOKABLE void openFiles(const QStringList &paths);
    Q_INVOKABLE bool openTerminal(const QString &dir); // $TERMINAL (or kitty) in `dir`

    // an Exec= line as program and arguments, `path` where it asks for a file (appended if it doesn't)
    static QStringList command(const QString &exec, const QString &path);

signals:
    void failed(const QString &message);
};
