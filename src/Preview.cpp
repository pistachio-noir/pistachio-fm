#include "Preview.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

bool runPdfTool(const QString &program, const QStringList &args,
                const std::shared_ptr<std::atomic<quint64>> &latest, quint64 generation, QByteArray *output = nullptr)
{
    if (*latest != generation)
        return false;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);
    process.setStandardErrorFile(QProcess::nullDevice());
    if (!output)
        process.setStandardOutputFile(QProcess::nullDevice());
    QElapsedTimer elapsed;
    elapsed.start();
    process.start(program, args);
    if (!process.waitForStarted(1000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(50);
        if (output) {
            // shortcut: retain 16 KiB of metadata; stream page-count fields if oversized metadata needs support.
            const QByteArray chunk = process.readAllStandardOutput();
            output->append(chunk.left(qMax(0, 16384 - int(output->size()))));
        }
        if (*latest != generation || elapsed.elapsed() > 10000) {
            process.kill();
            process.waitForFinished(1000);
            return false;
        }
    }
    if (output)
        output->append(process.readAllStandardOutput().left(qMax(0, 16384 - int(output->size()))));
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QVariantMap readPreview(const QString &path, int page,
                        const std::shared_ptr<std::atomic<quint64>> &latest, quint64 generation,
                        std::shared_ptr<QTemporaryDir> *rendered)
{
    const QFileInfo info(path);
    QVariantMap result = {
        { QStringLiteral("name"), info.fileName() },
        { QStringLiteral("url"), QUrl::fromLocalFile(path) },
        { QStringLiteral("kind"), QStringLiteral("error") },
        { QStringLiteral("note"), QStringLiteral("Couldn't read this file") },
    };
    if (!info.isFile())
        return result;

    // Nonblocking open plus fstat also rejects a file replaced by a FIFO between checks.
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return result;
    struct stat st;
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fd);
        return result;
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return result;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    if (mime.name() == QLatin1String("application/pdf")) {
        const QString pdfinfo = QStandardPaths::findExecutable(QStringLiteral("pdfinfo"));
        const QString pdftoppm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
        result[QStringLiteral("note")] = QStringLiteral("Couldn't preview this PDF (damaged, password protected, or too slow)");
        if (pdfinfo.isEmpty() || pdftoppm.isEmpty()) {
            result[QStringLiteral("note")] = QStringLiteral("PDF preview needs Poppler (pdfinfo and pdftoppm)");
            return result;
        }
        QByteArray metadata;
        if (!runPdfTool(pdfinfo, { path }, latest, generation, &metadata))
            return result;
        int pages = 0;
        for (const QByteArray &line : metadata.split('\n')) {
            if (line.startsWith("Pages:"))
                pages = line.mid(6).trimmed().toInt();
        }
        if (pages < 1)
            return result;
        page = qBound(1, page, pages);
        auto directory = std::make_shared<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/pfm-pdf-XXXXXX"));
        if (!directory->isValid())
            return result;
        const QString prefix = directory->filePath(QStringLiteral("page"));
        if (!runPdfTool(pdftoppm, { QStringLiteral("-f"), QString::number(page), QStringLiteral("-l"), QString::number(page),
                 QStringLiteral("-singlefile"), QStringLiteral("-scale-to"), QStringLiteral("1600"),
                 QStringLiteral("-png"), path, prefix }, latest, generation))
            return result;
        const QString image = prefix + QStringLiteral(".png");
        if (!QImageReader(image).canRead())
            return result;
        *rendered = directory;
        result[QStringLiteral("kind")] = QStringLiteral("pdf");
        result[QStringLiteral("url")] = QUrl::fromLocalFile(image);
        result[QStringLiteral("page")] = page;
        result[QStringLiteral("pages")] = pages;
        result[QStringLiteral("note")] = QString();
        return result;
    }
    if (QImageReader::supportedMimeTypes().contains(mime.name().toLatin1())) {
        result[QStringLiteral("kind")] = QStringLiteral("image");
        result[QStringLiteral("note")] = QString();
        return result;
    }
    result[QStringLiteral("kind")] = QStringLiteral("unsupported");
    result[QStringLiteral("note")] = QStringLiteral("No preview available for this file type");
    if (!mime.inherits(QStringLiteral("text/plain")) && !mime.inherits(QStringLiteral("application/json"))
        && mime.name() != QLatin1String("application/octet-stream")
        && mime.name() != QLatin1String("application/x-zerosize"))
        return result;

    // shortcut: preview only the first 128 KiB; add paged reading if large-file browsing is needed.
    constexpr qsizetype Limit = 128 * 1024;
    QByteArray bytes = file.read(Limit + 1);
    if (file.error() != QFileDevice::NoError) {
        result[QStringLiteral("kind")] = QStringLiteral("error");
        result[QStringLiteral("note")] = QStringLiteral("Couldn't read this file");
        return result;
    }
    const bool truncated = bytes.size() > Limit;
    if (truncated)
        bytes.truncate(Limit);
    const auto encoding = QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8);
    QStringDecoder decoder(encoding, truncated ? QStringConverter::Flag::Default : QStringConverter::Flag::Stateless);
    const QString text = decoder(bytes);
    if (decoder.hasError() || std::any_of(text.cbegin(), text.cend(), [](QChar c) {
            return c.category() == QChar::Other_Control && c != u'\t' && c != u'\r' && c != u'\n';
        }))
        return result;
    result[QStringLiteral("kind")] = QStringLiteral("text");
    result[QStringLiteral("text")] = text;
    result[QStringLiteral("note")] = truncated ? QStringLiteral("Showing the first 128 KiB")
        : text.isEmpty() ? QStringLiteral("Empty file") : QString();
    return result;
}

} // namespace

void Preview::load(const QString &path)
{
    m_path = QFileInfo(path).absoluteFilePath();
    m_content.clear();
    start(1);
}

void Preview::setPage(int page)
{
    if (m_loading || m_content.value(QStringLiteral("kind")).toString() != QLatin1String("pdf")
        || page < 1 || page > m_content.value(QStringLiteral("pages")).toInt()
        || page == m_content.value(QStringLiteral("page")).toInt())
        return;
    start(page);
}

void Preview::start(int page)
{
    const quint64 generation = ++*m_generation;
    const auto latest = m_generation;
    const QString path = m_path;
    const int pages = m_content.value(QStringLiteral("pages")).toInt();
    m_content = { { QStringLiteral("name"), QFileInfo(path).fileName() } };
    if (pages > 0) {
        m_content[QStringLiteral("kind")] = QStringLiteral("pdf");
        m_content[QStringLiteral("page")] = page;
        m_content[QStringLiteral("pages")] = pages;
    }
    m_rendered.reset();
    m_loading = true;
    emit changed();
    const QPointer<Preview> self(this);
    QThreadPool::globalInstance()->start([=] {
        if (*latest != generation)
            return;
        std::shared_ptr<QTemporaryDir> rendered;
        const QVariantMap result = readPreview(path, page, latest, generation, &rendered);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [=] {
            if (!self || *latest != generation)
                return;
            self->m_content = result;
            self->m_rendered = rendered;
            self->m_loading = false;
            emit self->changed();
        }, Qt::QueuedConnection);
    });
}

void Preview::clear()
{
    ++*m_generation;
    m_path.clear();
    m_content.clear();
    m_rendered.reset();
    m_loading = false;
    emit changed();
}
