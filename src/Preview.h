#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

class QTemporaryDir;

// One popup's read-only preview. Closing it invalidates any outstanding worker result.
class Preview : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap content READ content NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    explicit Preview(QObject *parent = nullptr) : QObject(parent) {}
    ~Preview() override { ++*m_generation; }
    QVariantMap content() const { return m_content; }
    bool loading() const { return m_loading; }
    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void setPage(int page);
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void start(int page);
    QString m_path;
    QVariantMap m_content;
    std::shared_ptr<QTemporaryDir> m_rendered;
    bool m_loading = false;
    std::shared_ptr<std::atomic<quint64>> m_generation = std::make_shared<std::atomic<quint64>>(0);
};
