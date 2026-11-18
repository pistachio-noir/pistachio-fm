#include "Apps.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#include <algorithm>

namespace {

struct Entry
{
    QString id; // "org.gnome.Loupe.desktop"
    QString name;
    QString icon;
    QString exec;
    QStringList types; // MimeType=
    bool terminal = false;
    bool shown = true; // not NoDisplay
};

// a value's escapes undone (\s \n \t \r \\), as the desktop entry spec writes strings
QString unescape(const QString &value)
{
    QString out;
    for (qsizetype i = 0; i < value.size(); ++i) {
        if (value[i] != u'\\' || i + 1 == value.size()) {
            out += value[i];
            continue;
        }
        const QChar next = value[++i];
        out += next == u's' ? u' ' : next == u'n' ? u'\n' : next == u't' ? u'\t' : next == u'r' ? u'\r' : next;
    }
    return out;
}

// one [group] of a desktop entry, mimeapps.list or mimeinfo.cache, as key -> value
QHash<QString, QString> group(const QString &file, const QString &name)
{
    QHash<QString, QString> keys;
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return keys;
    bool inside = false;
    while (!in.atEnd()) {
        const QString line = QString::fromUtf8(in.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            if (inside)
                break;
            inside = line == u'[' + name + u']';
        } else if (inside && !line.startsWith(u'#')) {
            const qsizetype equals = line.indexOf(u'=');
            if (equals > 0)
                keys.insert(line.left(equals).trimmed(), unescape(line.mid(equals + 1).trimmed()));
        }
    }
    return keys;
}

// every installed app, the first of an id winning (the user's own entries come first)
QList<Entry> entries()
{
    QList<Entry> list;
    QSet<QString> seen;
    const QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dir : dirs) {
        QDirIterator it(dir, { QStringLiteral("*.desktop") }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            const QString id = file.mid(dir.size() + 1).replace(u'/', u'-');
            if (seen.contains(id))
                continue;
            seen.insert(id); // a Hidden=true entry hides the system's one of the same id too
            const QHash<QString, QString> keys = group(file, QStringLiteral("Desktop Entry"));
            if (keys.value(QStringLiteral("Type")) != QLatin1String("Application")
                || keys.value(QStringLiteral("Hidden")) == QLatin1String("true") || keys.value(QStringLiteral("Exec")).isEmpty())
                continue;
            const QString tryExec = keys.value(QStringLiteral("TryExec"));
            if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty() && !QFileInfo(tryExec).isExecutable())
                continue;
            list.append({ id, keys.value(QStringLiteral("Name"), id), keys.value(QStringLiteral("Icon")),
                          keys.value(QStringLiteral("Exec")),
                          keys.value(QStringLiteral("MimeType")).split(u';', Qt::SkipEmptyParts),
                          keys.value(QStringLiteral("Terminal")) == QLatin1String("true"),
                          keys.value(QStringLiteral("NoDisplay")) != QLatin1String("true") });
        }
    }
    return list;
}

const Entry *find(const QList<Entry> &apps, const QString &id)
{
    const auto it = std::find_if(apps.cbegin(), apps.cend(), [&id](const Entry &app) { return app.id == id; });
    return it == apps.cend() ? nullptr : &*it;
}

// the type's own name and its aliases (mimeapps.list may use either)
QStringList namesOf(const QMimeType &mime)
{
    return QStringList { mime.name() } + mime.aliases();
}

// The default app for a type, as xdg-open finds it: [Default Applications] in each mimeapps.list (the
// user's first), then [Added Associations], then the system's mimeinfo.cache. The first one installed wins.
// shortcut: desktop-specific lists (hyprland-mimeapps.list) and [Removed Associations] are skipped; add if used
QString defaultId(const QMimeType &mime, const QList<Entry> &apps)
{
    QStringList lists;
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        lists.append(dir + QStringLiteral("/mimeapps.list"));
    const QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dir : appDirs)
        lists.append(dir + QStringLiteral("/mimeapps.list"));
    QList<std::pair<QStringList, QString>> searches = { { lists, QStringLiteral("Default Applications") },
                                                        { lists, QStringLiteral("Added Associations") } };
    QStringList caches;
    for (const QString &dir : appDirs)
        caches.append(dir + QStringLiteral("/mimeinfo.cache"));
    searches.append({ caches, QStringLiteral("MIME Cache") });

    for (const auto &[files, name] : std::as_const(searches)) {
        for (const QString &file : files) {
            const QHash<QString, QString> keys = group(file, name);
            for (const QString &type : namesOf(mime)) {
                for (const QString &id : keys.value(type).split(u';', Qt::SkipEmptyParts)) {
                    if (find(apps, id))
                        return id;
                }
            }
        }
    }
    return {};
}

// `mime=id` in [Default Applications] of the user's mimeapps.list; every other line stays as it was
bool setDefault(const QString &mime, const QString &id)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list");
    QStringList lines;
    {
        QFile in(path);
        if (in.open(QIODevice::ReadOnly | QIODevice::Text))
            lines = QString::fromUtf8(in.readAll()).split(u'\n');
    }
    const QString key = mime + u'=';
    const qsizetype start = std::find_if(lines.cbegin(), lines.cend(), [](const QString &line) {
                                return line.trimmed() == QLatin1String("[Default Applications]");
                            }) - lines.cbegin();
    if (start == lines.size()) {
        if (!lines.isEmpty() && lines.last().isEmpty())
            lines.removeLast();
        if (!lines.isEmpty())
            lines.append(QString());
        lines += { QStringLiteral("[Default Applications]"), key + id, QString() };
    } else {
        qsizetype end = start + 1;
        while (end < lines.size() && !lines[end].trimmed().startsWith(u'['))
            ++end;
        qsizetype at = -1;
        for (qsizetype i = start + 1; i < end; ++i) {
            if (lines[i].trimmed().startsWith(key))
                at = i;
        }
        if (at >= 0) {
            lines[at] = key + id;
        } else {
            qsizetype last = end; // after the group's last line, before the blank ones that end it
            while (last > start + 1 && lines[last - 1].trimmed().isEmpty())
                --last;
            lines.insert(last, key + id);
        }
    }
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile out(path); // written whole, then swapped in: a crash never leaves half a file
    return out.open(QIODevice::WriteOnly | QIODevice::Text) && out.write(lines.join(u'\n').toUtf8()) >= 0 && out.commit();
}

// the icon's name if the theme (or an absolute path) has it, "" so the card can show a letter instead
QString iconOf(const Entry &app)
{
    if (app.icon.startsWith(u'/'))
        return QFileInfo::exists(app.icon) ? app.icon : QString();
    return QIcon::hasThemeIcon(app.icon) ? app.icon : QString();
}

QVariantMap describe(const Entry &app, bool isDefault)
{
    return { { QStringLiteral("id"), app.id }, { QStringLiteral("name"), app.name },
             { QStringLiteral("icon"), iconOf(app) }, { QStringLiteral("isDefault"), isDefault },
             { QStringLiteral("terminal"), app.terminal } };
}

class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        const QIcon icon = id.startsWith(u'/') ? QIcon(id) : QIcon::fromTheme(id);
        const QPixmap pixmap = icon.pixmap(requested.isValid() ? requested : QSize(48, 48));
        if (size)
            *size = pixmap.size();
        return pixmap;
    }
};

// image://thumbnail/<percent-encoded path>: a picture decoded at the size asked for (JPEGs are decoded small to
// begin with), cut to the round badge here, so no shader is needed
class ThumbnailProvider : public QQuickImageProvider
{
public:
    ThumbnailProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override
    {
        const int side = requested.width() > 0 ? requested.width() : 72;
        QImageReader reader(QUrl::fromPercentEncoding(id.toUtf8()));
        reader.setAutoTransform(true);
        const QSize full = reader.size();
        if (full.isValid())
            reader.setScaledSize(full.scaled(side, side, Qt::KeepAspectRatioByExpanding));
        const QImage picture = reader.read();
        if (picture.isNull())
            return {};
        QImage round(side, side, QImage::Format_ARGB32_Premultiplied);
        round.fill(Qt::transparent);
        QPainter painter(&round);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath circle;
        circle.addEllipse(0, 0, side, side);
        painter.setClipPath(circle);
        const QImage filled = picture.size().width() < side || picture.size().height() < side
            ? picture.scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation) : picture;
        painter.drawImage((side - filled.width()) / 2, (side - filled.height()) / 2, filled);
        painter.end();
        if (size)
            *size = round.size();
        return round;
    }
};

} // namespace

// the terminal, as the shell's launcher picks it
static QString terminal()
{
    const QString terminal = qEnvironmentVariable("TERMINAL");
    return terminal.isEmpty() ? QStringLiteral("kitty") : terminal;
}

// starts `app` on `path`, in its folder; a terminal app in the terminal
static bool launch(const Entry &app, const QString &path)
{
    QStringList program = Apps::command(app.exec, path);
    if (app.terminal)
        program = QStringList { terminal(), QStringLiteral("-e") } + program;
    return !program.isEmpty() && QProcess::startDetached(program.first(), program.mid(1), QFileInfo(path).path());
}

Apps::Apps(QObject *parent)
    : QObject(parent)
{
}

Apps *Apps::create(QQmlEngine *engine, QJSEngine *)
{
    engine->addImageProvider(QStringLiteral("appicon"), new IconProvider);
    engine->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    return new Apps(nullptr);
}

QVariantMap Apps::buildInfo() const
{
    return { { QStringLiteral("compiler"), QStringLiteral("Clang ") + QString::fromLatin1(__clang_version__) },
             { QStringLiteral("qt"), QString::fromLatin1(qVersion()) },
             { QStringLiteral("meson"), QStringLiteral(PFM_MESON_VERSION) },
             { QStringLiteral("system"), QSysInfo::prettyProductName() },
             { QStringLiteral("kernel"), QSysInfo::kernelType() + u' ' + QSysInfo::kernelVersion() },
             { QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture() } };
}

QVariantMap Apps::forFile(const QString &path) const
{
    return forMime(QMimeDatabase().mimeTypeForFile(path).name());
}

QVariantMap Apps::forMime(const QString &type) const
{
    const QMimeType mime = QMimeDatabase().mimeTypeForName(type);
    const QList<Entry> apps = entries();
    const QString fallback = mime.isValid() ? defaultId(mime, apps) : QString();
    const QStringList own = mime.isValid() ? namesOf(mime) : QStringList { type };
    const QStringList ancestors = mime.allAncestors();

    QList<std::pair<int, const Entry *>> ranked; // 0 the default, 1 for the type, 2 for what it's a kind of
    for (const Entry &app : apps) {
        const auto handles = [&app](const QStringList &types) {
            return std::any_of(types.cbegin(), types.cend(), [&app](const QString &type) { return app.types.contains(type); });
        };
        if (app.id == fallback)
            ranked.append({ 0, &app });
        else if (handles(own))
            ranked.append({ 1, &app });
        else if (handles(ancestors))
            ranked.append({ 2, &app });
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(ranked.begin(), ranked.end(), [&collator](const auto &a, const auto &b) {
        return a.first != b.first ? a.first < b.first : collator.compare(a.second->name, b.second->name) < 0;
    });
    QVariantList list;
    for (const auto &[rank, app] : std::as_const(ranked))
        list.append(describe(*app, rank == 0));
    return { { QStringLiteral("mime"), mime.isValid() ? mime.name() : type },
             { QStringLiteral("type"), mime.isValid() ? mime.comment() : type },
             { QStringLiteral("apps"), list } };
}

QVariantList Apps::all() const
{
    QList<Entry> apps = entries();
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(apps.begin(), apps.end(), [&collator](const Entry &a, const Entry &b) { return collator.compare(a.name, b.name) < 0; });
    QVariantList list;
    for (const Entry &app : std::as_const(apps)) {
        if (app.shown)
            list.append(describe(app, false));
    }
    return list;
}

QString Apps::defaultName(const QString &path) const
{
    const QList<Entry> apps = entries();
    const Entry *app = find(apps, defaultId(QMimeDatabase().mimeTypeForFile(path), apps));
    return app ? app->name : QString();
}

bool Apps::open(const QString &id, const QString &path, bool remember)
{
    const QList<Entry> apps = entries();
    const Entry *app = find(apps, id);
    if (!app) {
        emit failed(QStringLiteral("That app isn't installed any more"));
        return false;
    }
    if (!launch(*app, path)) {
        emit failed(QStringLiteral("Couldn't start %1").arg(app->name));
        return false;
    }
    if (remember && !setDefault(QMimeDatabase().mimeTypeForFile(path).name(), id))
        emit failed(QStringLiteral("Couldn't save the default in ~/.config/mimeapps.list"));
    return true;
}

void Apps::openFiles(const QStringList &paths)
{
    const QList<Entry> apps = entries();
    const QMimeDatabase mimes;
    for (const QString &path : paths) {
        const Entry *app = find(apps, defaultId(mimes.mimeTypeForFile(path), apps));
        if (!app)
            emit failed(QStringLiteral("Nothing opens “%1” yet: right-click it, Open with…").arg(QFileInfo(path).fileName()));
        else if (!launch(*app, path))
            emit failed(QStringLiteral("Couldn't start %1").arg(app->name));
    }
}

QStringList Apps::command(const QString &exec, const QString &path)
{
    // arguments split at spaces; "…" keeps them, and \" \` \$ \\ inside it stand for themselves
    QStringList args;
    QString arg;
    bool quoted = false;
    bool started = false;
    for (qsizetype i = 0; i < exec.size(); ++i) {
        const QChar c = exec[i];
        if (quoted && c == u'\\' && i + 1 < exec.size()) {
            arg += exec[++i];
        } else if (c == u'"') {
            quoted = !quoted;
            started = true;
        } else if (c == u' ' && !quoted) {
            if (started || !arg.isEmpty())
                args.append(arg);
            arg.clear();
            started = false;
        } else {
            arg += c;
        }
    }
    if (started || !arg.isEmpty())
        args.append(arg);

    // field codes: the file for %f %F (a path) and %u %U (a URL), %% for %, the others dropped
    // shortcut: %i (icon), %c (name) and %k (entry's path) are dropped too; few apps need them
    const QString url = QString::fromUtf8(QUrl::fromLocalFile(path).toEncoded());
    QStringList program;
    bool placed = false;
    for (const QString &word : std::as_const(args)) {
        QString expanded;
        for (qsizetype i = 0; i < word.size(); ++i) {
            if (word[i] != u'%' || i + 1 == word.size()) {
                expanded += word[i];
                continue;
            }
            const QChar code = word[++i];
            if (code == u'%') {
                expanded += u'%';
            } else if (code == u'f' || code == u'F') {
                expanded += path;
                placed = true;
            } else if (code == u'u' || code == u'U') {
                expanded += url;
                placed = true;
            }
        }
        if (!expanded.isEmpty() || !word.contains(u'%'))
            program.append(expanded);
    }
    if (!placed && !program.isEmpty())
        program.append(path); // an app that takes no file still gets the one picked for it
    return program;
}

bool Apps::openTerminal(const QString &dir)
{
    if (QProcess::startDetached(terminal(), {}, dir))
        return true;
    emit failed(QStringLiteral("Couldn't start %1").arg(terminal()));
    return false;
}
