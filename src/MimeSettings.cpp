#include "MimeSettings.h"

#include "Apps.h"

#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

MimeSettings::MimeSettings(QObject *parent) : QObject(parent)
{
    const QStringList popular = {
        QStringLiteral("text/plain"), QStringLiteral("text/markdown"), QStringLiteral("application/pdf"),
        QStringLiteral("image/png"), QStringLiteral("image/jpeg"), QStringLiteral("image/webp"),
        QStringLiteral("image/svg+xml"), QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"),
        QStringLiteral("video/mp4"), QStringLiteral("video/webm"), QStringLiteral("application/zip"),
        QStringLiteral("text/html"), QStringLiteral("text/csv"), QStringLiteral("application/json"),
        QStringLiteral("inode/directory"),
    };
    const QMimeDatabase database;
    auto types = database.allMimeTypes();
    std::sort(types.begin(), types.end(), [](const QMimeType &a, const QMimeType &b) { return a.name() < b.name(); });
    const auto append = [this](const QMimeType &type, bool common) {
        m_types.append(QVariantMap { { QStringLiteral("mime"), type.name() },
            { QStringLiteral("name"), type.comment() }, { QStringLiteral("popular"), common } });
    };
    for (const QString &name : popular) {
        const auto type = database.mimeTypeForName(name);
        if (type.isValid())
            append(type, true);
    }
    for (const QMimeType &type : std::as_const(types)) {
        if (!popular.contains(type.name()))
            append(type, false);
    }
}

void MimeSettings::fail(const QString &message)
{
    m_error = message;
    m_message.clear();
    m_busy = false;
    m_saving = false;
    emit changed();
}

void MimeSettings::inspect(const QString &mime)
{
    if (m_saving)
        return;
    const quint64 request = ++m_request;
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
    const QString type = mime.trimmed().toLower();
    static const QRegularExpression valid(QStringLiteral("\\A[a-z0-9][a-z0-9!#$&^_.+-]*/[a-z0-9][a-z0-9!#$&^_.+-]*\\z"));
    if (!valid.match(type).hasMatch()) {
        fail(QStringLiteral("Enter a MIME type such as application/pdf"));
        return;
    }
    Apps apps(nullptr);
    m_info = apps.forMime(type);
    const QVariantList recommended = m_info.value(QStringLiteral("apps")).toList();
    QVariantList all = recommended;
    QSet<QString> seen;
    for (const QVariant &app : all)
        seen.insert(app.toMap().value(QStringLiteral("id")).toString());
    for (const QVariant &app : apps.all()) {
        if (!seen.contains(app.toMap().value(QStringLiteral("id")).toString()))
            all.append(app);
    }
    m_info[QStringLiteral("apps")] = all;
    m_info[QStringLiteral("recommended")] = recommended;
    m_error.clear();
    m_message.clear();
    m_busy = true;
    emit changed();
    run({ QStringLiteral("query"), QStringLiteral("default"), m_info.value(QStringLiteral("mime")).toString() }, request);
}

void MimeSettings::save(const QString &id)
{
    if (m_busy)
        return;
    Apps apps(nullptr);
    const QString mime = m_info.value(QStringLiteral("mime")).toString();
    const auto available = apps.forMime(mime).value(QStringLiteral("apps")).toList() + apps.all();
    const bool installed = std::any_of(available.cbegin(), available.cend(), [&id](const QVariant &app) {
        return app.toMap().value(QStringLiteral("id")).toString() == id;
    });
    if (mime.isEmpty() || !installed || id.startsWith(u'-') || id.contains(u'\n')) {
        fail(QStringLiteral("Choose an installed application first"));
        return;
    }
    m_error.clear();
    m_message.clear();
    m_busy = true;
    m_saving = true;
    emit changed();
    run({ QStringLiteral("default"), id, mime }, ++m_request, id);
}

void MimeSettings::run(const QStringList &args, quint64 request, const QString &verify)
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (executable.isEmpty()) {
        fail(QStringLiteral("Default applications need xdg-mime (xdg-utils)"));
        return;
    }
    auto *process = new QProcess(this);
    m_process = process;
    process->setStandardErrorFile(QProcess::nullDevice());
    connect(process, &QProcess::errorOccurred, this, [=, this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            if (request == m_request)
                fail(QStringLiteral("Couldn't start xdg-mime"));
        }
    });
    connect(process, &QProcess::finished, this, [=, this](int code, QProcess::ExitStatus status) {
        const QString id = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();
        if (request != m_request)
            return;
        if (status != QProcess::NormalExit || code != 0) {
            fail(QStringLiteral("xdg-mime failed or took too long"));
            return;
        }
        if (args.first() == QLatin1String("default")) {
            run({ QStringLiteral("query"), QStringLiteral("default"), m_info.value(QStringLiteral("mime")).toString() }, request, verify);
            return;
        }
        if (!id.isEmpty() && (!id.endsWith(QLatin1String(".desktop")) || id.contains(u'\n'))) {
            fail(QStringLiteral("Unexpected response from xdg-mime"));
            return;
        }
        QString name = id.isEmpty() ? QStringLiteral("None") : id;
        QVariantList recommended = m_info.value(QStringLiteral("recommended")).toList();
        QVariantList all = m_info.value(QStringLiteral("apps")).toList();
        for (qsizetype i = 0; i < all.size(); ++i) {
            const QVariant app = all.at(i);
            if (app.toMap().value(QStringLiteral("id")).toString() == id) {
                name = app.toMap().value(QStringLiteral("name")).toString();
                recommended.removeAll(app);
                recommended.prepend(app);
                all.removeAt(i);
                all.prepend(app);
                break;
            }
        }
        m_info[QStringLiteral("apps")] = all;
        m_info[QStringLiteral("recommended")] = recommended;
        m_info[QStringLiteral("currentId")] = id;
        m_info[QStringLiteral("currentName")] = name;
        m_busy = false;
        m_saving = false;
        if (!verify.isEmpty()) {
            if (id != verify)
                m_error = QStringLiteral("The effective default did not change; a desktop-specific override may take priority");
            else
                m_message = QStringLiteral("Default application updated");
        }
        emit changed();
    });
    QTimer::singleShot(5000, process, [process] {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
    process->start(executable, args);
}
