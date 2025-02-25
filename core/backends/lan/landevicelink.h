/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef LANDEVICELINK_H
#define LANDEVICELINK_H

#include <QObject>
#include <QPointer>
#include <QSslSocket>
#include <QString>

#include "backends/devicelink.h"
#include "compositeuploadjob.h"
#include "deviceinfo.h"
#include "uploadjob.h"
#include <kdeconnectcore_export.h>

class SocketLineReader;
class LanLinkProvider;

class KDECONNECTCORE_EXPORT LanDeviceLink : public DeviceLink
{
    Q_OBJECT

public:
    enum ConnectionStarted : bool { Locally, Remotely };

    LanDeviceLink(const DeviceInfo &deviceInfo, LanLinkProvider *parent, QSslSocket *socket);
    void reset(QSslSocket *socket);

    bool sendPacket(NetworkPacket &np) override;

    DeviceInfo deviceInfo() const override
    {
        return m_deviceInfo;
    }

    QHostAddress hostAddress() const;

private Q_SLOTS:
    void dataReceived();

private:
    SocketLineReader *m_socketLineReader;
    ConnectionStarted m_connectionSource;
    QPointer<CompositeUploadJob> m_compositeUploadJob;
    DeviceInfo m_deviceInfo;
};

#endif
