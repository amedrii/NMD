#include "mainwindow.h"
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("NMD");
    QCoreApplication::setApplicationName("Nexus Mod Downloader");
    QCoreApplication::setApplicationVersion("0.1.0");
    const QString serverName = "NMD-" + qEnvironmentVariable("USERNAME", "local");
    QString incoming;
    for (const auto& arg : app.arguments().mid(1))
    {
        if (arg.startsWith("nxm://", Qt::CaseInsensitive))
        {
            incoming = arg;
        }
    }
    QLocalSocket client;
    client.connectToServer(serverName);
    if (client.waitForConnected(400))
    {
        client.write(incoming.toUtf8() + '\n');
        client.waitForBytesWritten(1000);
        return 0;
    }
    QLocalServer server;
    QLocalServer::removeServer(serverName);
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(serverName))
    {
        return 1;
    }
    MainWindow window;
    QObject::connect(&server,
        &QLocalServer::newConnection,
        &window,
        [&]
    {
        while (auto socket = server.nextPendingConnection())
        {
            QObject::connect(socket,
                &QLocalSocket::readyRead,
                &window,
                [&, socket]
            {
                if (socket->bytesAvailable() > 16384)
                {
                    socket->abort();
                    return;
                }
                if (!socket->canReadLine())
                {
                    return;
                }
                const auto link = QString::fromUtf8(socket->readLine()).trimmed();
                window.showNormal();
                window.raise();
                window.activateWindow();
                if (!link.isEmpty())
                {
                    window.acceptDownloadLink(link);
                }
                socket->disconnectFromServer();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    window.show();
    if (!incoming.isEmpty())
    {
        QTimer::singleShot(0, &window, [&] { window.acceptDownloadLink(incoming); });
    }
    return app.exec();
}
