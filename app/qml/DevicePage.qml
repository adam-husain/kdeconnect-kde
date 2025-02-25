/*
 * SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.0
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kdeconnect 1.0

Kirigami.ScrollablePage {
    id: root
    property QtObject currentDevice
    title: currentDevice.name

    actions.contextualActions: [
        Kirigami.Action {
            iconName:"network-disconnect"
            onTriggered: root.currentDevice.unpair()
            text: i18nd("kdeconnect-app", "Unpair")
            visible: root.currentDevice.isPaired
        },
        Kirigami.Action {
            iconName:"hands-free"
            text: i18nd("kdeconnect-app", "Send Ping")
            visible: root.currentDevice.isPaired && root.currentDevice.isReachable
            onTriggered: {
                root.currentDevice.pluginCall("ping", "sendPing");
            }
        },
        Kirigami.Action {
            iconName: "settings-configure"
            text: i18n("Plugin Settings")
            visible: root.currentDevice.isPaired && root.currentDevice.isReachable
            onTriggered: {
                pageStack.push(
                    Qt.resolvedUrl("PluginSettings.qml"),
                    {device: currentDevice.id()}
                );
            }
        }
    ]

    ListView {
        model: plugins
        delegate: Kirigami.BasicListItem {
            label: name
            icon: iconName
            highlighted: false
            iconColor: "transparent"
            visible: loaded
            onClicked: onClick()
        }

        property list<QtObject> plugins: [
            PluginItem {
                name: i18nd("kdeconnect-app", "Multimedia control")
                interfaceFactory: MprisDbusInterfaceFactory
                component: "qrc:/qml/mpris.qml"
                pluginName: "mprisremote"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Remote input")
                interfaceFactory: RemoteControlDbusInterfaceFactory
                component: "qrc:/qml/mousepad.qml"
                pluginName: "remotecontrol"
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Presentation Remote")
                interfaceFactory: RemoteKeyboardDbusInterfaceFactory
                component: "qrc:/qml/presentationRemote.qml"
                pluginName: "remotecontrol"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var lockIface: LockDeviceDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "lockdevice"
                name: lockIface.isLocked ? i18nd("kdeconnect-app", "Unlock") : i18nd("kdeconnect-app", "Lock")
                onClick: () => lockIface.isLocked = !lockIface.isLocked;
                device: root.currentDevice
            },
            PluginItem {
                readonly property var findmyphoneIface: FindMyPhoneDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "findmyphone"
                name: i18nd("kdeconnect-app", "Find Device")
                onClick: () => findmyphoneIface.ring()
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Run command")
                interfaceFactory: RemoteCommandsDbusInterfaceFactory
                component: "qrc:/qml/runcommand.qml"
                pluginName: "remotecommands"
                device: root.currentDevice
            },
            PluginItem {
                readonly property var clipboardIface: ClipboardDbusInterfaceFactory.create(root.currentDevice.id())
                pluginName: "clipboard"
                name: i18nd("kdeconnect-app", "Send Clipboard")
                onClick: () => clipboardIface.sendClipboard()
                device: root.currentDevice
            },
            PluginItem {
                pluginName: "share"
                name: i18nd("kdeconnect-app", "Share File")
                onClick: () => fileDialog.open()
                device: root.currentDevice
            },
            PluginItem {
                name: i18nd("kdeconnect-app", "Volume control")
                interfaceFactory: RemoteSystemVolumeDbusInterfaceFactory
                component: "qrc:/qml/volume.qml"
                pluginName: "remotesystemvolume"
                device: root.currentDevice
            }
        ]

        Kirigami.PlaceholderMessage {
            text: i18nd("kdeconnect-app", "This device is not paired")
            anchors.centerIn: parent
            visible: root.currentDevice && root.currentDevice.isReachable && !root.currentDevice.isPaired && !root.currentDevice.isPairRequestedByPeer && !root.currentDevice.isPairRequested
            helpfulAction: Kirigami.Action {
                text: i18nd("kdeconnect-app", "Pair")
                icon.name:"network-connect"
                onTriggered: root.currentDevice.requestPairing()
            }
        }

        Kirigami.PlaceholderMessage {
            text: i18nd("kdeconnect-app", "Pair requested")
            anchors.centerIn: parent
            visible: root.currentDevice && root.currentDevice.isReachable && root.currentDevice.isPairRequested
            QQC2.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Kirigami.PlaceholderMessage {
            text: i18n("Pair requested")
            visible: root.currentDevice && root.currentDevice.isPairRequestedByPeer
            anchors.centerIn: parent
            spacing: Kirigami.Units.largeSpacing
            RowLayout {
                QQC2.Button {
                    text: i18nd("kdeconnect-app", "Accept")
                    icon.name:"dialog-ok"
                    onClicked: root.currentDevice.acceptPairing()
                }

                QQC2.Button {
                    text: i18nd("kdeconnect-app", "Reject")
                    icon.name:"dialog-cancel"
                    onClicked: root.currentDevice.cancelPairing()
                }
            }
        }

        Kirigami.PlaceholderMessage {
            visible: root.currentDevice && !root.currentDevice.isReachable
            text: i18nd("kdeconnect-app", "This device is not reachable")
            anchors.centerIn: parent
        }
    }

    FileDialog {
        id: fileDialog
        readonly property var shareIface: root.currentDevice ? ShareDbusInterfaceFactory.create(root.currentDevice.id()) : null
        title: i18nd("kdeconnect-app", "Please choose a file")
        folder: shortcuts.home
        onAccepted: shareIface.shareUrl(fileDialog.fileUrl)
    }
}
