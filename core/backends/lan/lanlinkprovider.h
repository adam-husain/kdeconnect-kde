/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef LANLINKPROVIDER_H
#define LANLINKPROVIDER_H

#include <QNetworkSession>
#include <QObject>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>

#include "backends/linkprovider.h"
#include "kdeconnectcore_export.h"
#include "landevicelink.h"
#include "server.h"

class KDECONNECTCORE_EXPORT LanLinkProvider : public LinkProvider
{
    Q_OBJECT

public:
    /**
     * @param testMode Some special overrides needed while testing
     * @param udpBroadcastPort Port which should be used for *sending* identity packets
     * @param udpListenPort Port which should be used for *receiving* identity packets
     */
    LanLinkProvider(bool testMode = false, quint16 udpBroadcastPort = UDP_PORT, quint16 udpListenPort = UDP_PORT);
    ~LanLinkProvider() override;

    QString name() override
    {
        return QStringLiteral("LanLinkProvider");
    }

    static void configureSslSocket(QSslSocket *socket, const QString &deviceId, bool isDeviceTrusted);
    static void configureSocket(QSslSocket *socket);

    /**
     * This is the default UDP port both for broadcasting and receiving identity packets
     */
    const static quint16 UDP_PORT = 1716;
    const static quint16 MIN_TCP_PORT = 1716;
    const static quint16 MAX_TCP_PORT = 1764;

public Q_SLOTS:
    void onNetworkChange() override;
    void onLinkDestroyed(const QString &deviceId, DeviceLink *oldPtr) override;
    void onStart() override;
    void onStop() override;
    void tcpSocketConnected();
    void encrypted();
    void connectError(QAbstractSocket::SocketError socketError);

private Q_SLOTS:
    void udpBroadcastReceived();
    void newConnection();
    void dataReceived();
    void sslErrors(const QList<QSslError> &errors);
    void broadcastToNetwork();

private:
    void onNetworkConfigurationChanged(const QNetworkConfiguration &config);
    void addLink(QSslSocket *socket, const DeviceInfo &deviceInfo);
    QList<QHostAddress> getBroadcastAddresses();
    void sendBroadcasts(QUdpSocket &socket, const NetworkPacket &np, const QList<QHostAddress> &addresses);

    Server *m_server;
    QUdpSocket m_udpSocket;
    quint16 m_tcpPort;

    quint16 m_udpBroadcastPort;
    quint16 m_udpListenPort;

    QMap<QString, LanDeviceLink *> m_links;

    struct PendingConnect {
        NetworkPacket *np;
        QHostAddress sender;
    };
    QMap<QSslSocket *, PendingConnect> m_receivedIdentityPackets;
    QNetworkConfiguration m_lastConfig;
    const bool m_testMode;
    QTimer m_combineBroadcastsTimer;
};

#endif
