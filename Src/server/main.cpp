#include "trainingserver.h"

#include <QCoreApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fau-training-server"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("port"), QStringLiteral("TCP port (default 8097)"), QStringLiteral("port"), QStringLiteral("8097")});
    parser.addOption({QStringLiteral("root"), QStringLiteral("Project data root"), QStringLiteral("path"), QCoreApplication::applicationDirPath()});
    parser.process(application);
    bool ok = false;
    const quint16 port = parser.value(QStringLiteral("port")).toUShort(&ok);
    if (!ok || port == 0) {
        qCritical("Invalid TCP port");
        return 2;
    }
    TrainingServer server(parser.value(QStringLiteral("root")));
    if (!server.listen(QHostAddress::LocalHost, port)) {
        qCritical("Could not listen on the requested local TCP port");
        return 1;
    }
    qInfo().noquote() << "FAU training server listening on 127.0.0.1:" << port;
    return application.exec();
}
