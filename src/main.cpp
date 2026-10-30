#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Pistachio_FilesPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("pistachio-fm"));
    app.setApplicationDisplayName(QStringLiteral("Files"));
    app.setApplicationVersion(QStringLiteral(PFM_VERSION));
    app.setDesktopFileName(QStringLiteral("pistachio-fm")); // the Wayland app id, for window rules

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A file manager in Pistachio Noir."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("place"),
        QStringLiteral("A folder to open (path or file:// URL); a file opens its folder with it selected."));
    parser.process(app);

    // xdg-open hands over URLs, a terminal plain or relative paths
    QString start = QDir::homePath();
    if (const QStringList args = parser.positionalArguments(); !args.isEmpty()) {
        const QUrl url = QUrl::fromUserInput(args.first(), QDir::currentPath(), QUrl::AssumeLocalFile);
        if (url.isLocalFile())
            start = url.toLocalFile();
    }

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("startPath"), start } });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Pistachio.Files", "Main");
    return app.exec();
}
