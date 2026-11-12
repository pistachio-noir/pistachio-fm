#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QProcess;

// Desktop-wide defaults through xdg-mime; selecting a type never changes an association.
class MimeSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList types READ types CONSTANT)
    Q_PROPERTY(QVariantMap info READ info NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    explicit MimeSettings(QObject *parent = nullptr);
    QVariantList types() const { return m_types; }
    QVariantMap info() const { return m_info; }
    bool busy() const { return m_busy; }
    bool saving() const { return m_saving; }
    QString error() const { return m_error; }
    QString message() const { return m_message; }
    Q_INVOKABLE void inspect(const QString &mime);
    Q_INVOKABLE void save(const QString &id);

signals:
    void changed();

private:
    void run(const QStringList &args, quint64 request, const QString &verify = {});
    void fail(const QString &message);
    QVariantList m_types;
    QVariantMap m_info;
    QPointer<QProcess> m_process;
    quint64 m_request = 0;
    bool m_busy = false;
    bool m_saving = false;
    QString m_error;
    QString m_message;
};
