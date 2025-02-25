/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kcm.h"

#include <KAboutData>
#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <kcmutils_version.h>

#include "dbusinterfaces.h"
#include "devicesmodel.h"
#include "devicessortproxymodel.h"
#include "kdeconnect-version.h"
#include "ui_kcm.h"

K_PLUGIN_CLASS_WITH_JSON(KdeConnectKcm, "kcm_kdeconnect.json")

static QString createId()
{
    return QStringLiteral("kcm") + QString::number(QCoreApplication::applicationPid());
}

KdeConnectKcm::KdeConnectKcm(QWidget *parent, const QVariantList &args)
    : KCModule(parent)
    , kcmUi(new Ui::KdeConnectKcmUi())
    , daemon(new DaemonDbusInterface(this))
    , devicesModel(new DevicesModel(this))
    , currentDevice(nullptr)
{
    KAboutData *about = new KAboutData(QStringLiteral("kdeconnect-kcm"),
                                       i18n("KDE Connect Settings"),
                                       QStringLiteral(KDECONNECT_VERSION_STRING),
                                       i18n("KDE Connect Settings module"),
                                       KAboutLicense::KAboutLicense::GPL_V2,
                                       i18n("(C) 2015 Albert Vaca Cintora"),
                                       QString(),
                                       QStringLiteral("https://community.kde.org/KDEConnect"));
    about->addAuthor(i18n("Albert Vaca Cintora"));
    about->setProgramLogo(QIcon(QStringLiteral(":/icons/kdeconnect/kdeconnect.svg")));

    setAboutData(about);

#ifdef Q_OS_WIN
    KColorSchemeManager manager;
    QApplication::setStyle(QStringLiteral("breeze"));
#endif

    kcmUi->setupUi(this);

    sortProxyModel = new DevicesSortProxyModel(devicesModel);

    kcmUi->deviceList->setModel(sortProxyModel);

    kcmUi->deviceInfo->setVisible(false);
    kcmUi->progressBar->setVisible(false);
    kcmUi->messages->setVisible(false);

    // Workaround: If we set this directly (or if we set it in the .ui file), the layout breaks
    kcmUi->noDeviceLinks->setWordWrap(false);
    QTimer::singleShot(0, this, [this] {
        kcmUi->noDeviceLinks->setWordWrap(true);
    });

    setWhenAvailable(
        daemon->announcedName(),
        [this](const QString &announcedName) {
            kcmUi->rename_label->setText(announcedName);
            kcmUi->rename_edit->setText(announcedName);
        },
        this);
    connect(daemon, SIGNAL(announcedNameChanged(QString)), kcmUi->rename_edit, SLOT(setText(QString)));
    connect(daemon, SIGNAL(announcedNameChanged(QString)), kcmUi->rename_label, SLOT(setText(QString)));
    setRenameMode(false);

    setButtons(KCModule::Help | KCModule::NoAdditionalButton);

    connect(devicesModel, &QAbstractItemModel::dataChanged, this, &KdeConnectKcm::resetSelection);
    connect(kcmUi->deviceList->selectionModel(), &QItemSelectionModel::currentChanged, this, &KdeConnectKcm::deviceSelected);
    connect(kcmUi->accept_button, &QAbstractButton::clicked, this, &KdeConnectKcm::acceptPairing);
    connect(kcmUi->reject_button, &QAbstractButton::clicked, this, &KdeConnectKcm::cancelPairing);
    connect(kcmUi->cancel_button, &QAbstractButton::clicked, this, &KdeConnectKcm::cancelPairing);
    connect(kcmUi->pair_button, &QAbstractButton::clicked, this, &KdeConnectKcm::requestPairing);
    connect(kcmUi->unpair_button, &QAbstractButton::clicked, this, &KdeConnectKcm::unpair);
    connect(kcmUi->ping_button, &QAbstractButton::clicked, this, &KdeConnectKcm::sendPing);
    connect(kcmUi->refresh_button, &QAbstractButton::clicked, this, &KdeConnectKcm::refresh);
    connect(kcmUi->rename_edit, &QLineEdit::returnPressed, this, &KdeConnectKcm::renameDone);
    connect(kcmUi->renameDone_button, &QAbstractButton::clicked, this, &KdeConnectKcm::renameDone);
    connect(kcmUi->renameShow_button, &QAbstractButton::clicked, this, &KdeConnectKcm::renameShow);
    connect(kcmUi->pluginSelector, &KPluginWidget::changed, this, &KdeConnectKcm::pluginsConfigChanged);

#if KCMUTILS_VERSION >= QT_VERSION_CHECK(5, 45, 0)

    if (!args.isEmpty() && args.first().type() == QVariant::String) {
        const QString input = args.first().toString();
        const auto colonIdx = input.indexOf(QLatin1Char(':'));
        const QString deviceId = input.left(colonIdx);
        const QString pluginCM = colonIdx < 0 ? QString() : input.mid(colonIdx + 1);

        connect(devicesModel, &DevicesModel::rowsInserted, this, [this, deviceId, pluginCM]() {
            auto row = devicesModel->rowForDevice(deviceId);
            if (row >= 0) {
                const QModelIndex idx = sortProxyModel->mapFromSource(devicesModel->index(row));
                kcmUi->deviceList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
            }
            if (!pluginCM.isEmpty()) {
                kcmUi->pluginSelector->showConfiguration(pluginCM);
            }
            disconnect(devicesModel, &DevicesModel::rowsInserted, this, nullptr);
        });
    }

#endif
}

void KdeConnectKcm::renameShow()
{
    setRenameMode(true);
}

void KdeConnectKcm::renameDone()
{
    QString newName = kcmUi->rename_edit->text();
    if (newName.isEmpty()) {
        // Rollback changes
        kcmUi->rename_edit->setText(kcmUi->rename_label->text());
    } else {
        kcmUi->rename_label->setText(newName);
        daemon->setAnnouncedName(newName);
    }
    setRenameMode(false);
}

void KdeConnectKcm::setRenameMode(bool b)
{
    kcmUi->renameDone_button->setVisible(b);
    kcmUi->rename_edit->setVisible(b);
    kcmUi->renameShow_button->setVisible(!b);
    kcmUi->rename_label->setVisible(!b);
}

KdeConnectKcm::~KdeConnectKcm()
{
    delete kcmUi;
}

void KdeConnectKcm::refresh()
{
    daemon->forceOnNetworkChange();
}

void KdeConnectKcm::resetSelection()
{
    if (!currentDevice) {
        return;
    }
    kcmUi->deviceList->selectionModel()->setCurrentIndex(sortProxyModel->mapFromSource(currentIndex), QItemSelectionModel::ClearAndSelect);
}

void KdeConnectKcm::deviceSelected(const QModelIndex &current)
{
    if (currentDevice) {
        disconnect(currentDevice, 0, this, 0);
    }

    if (!current.isValid()) {
        currentDevice = nullptr;
        kcmUi->deviceInfo->setVisible(false);
        return;
    }

    currentIndex = sortProxyModel->mapToSource(current);
    currentDevice = devicesModel->getDevice(currentIndex.row());

    kcmUi->noDevicePlaceholder->setVisible(false);
    bool valid = (currentDevice != nullptr && currentDevice->isValid());
    kcmUi->deviceInfo->setVisible(valid);
    if (!valid) {
        return;
    }

    kcmUi->messages->setVisible(false);
    resetDeviceView();

    connect(currentDevice, SIGNAL(pluginsChanged()), this, SLOT(resetCurrentDevice()));
    connect(currentDevice, SIGNAL(pairingFailed(QString)), this, SLOT(pairingFailed(QString)));
    connect(currentDevice, &DeviceDbusInterface::pairStateChangedProxy, this, &KdeConnectKcm::setCurrentDevicePairState);
}

void KdeConnectKcm::resetCurrentDevice()
{
    const QStringList supportedPluginNames = currentDevice->supportedPlugins();

    if (m_oldSupportedPluginNames != supportedPluginNames) {
        resetDeviceView();
    }
}

void KdeConnectKcm::resetDeviceView()
{
    kcmUi->verificationKey->setText(i18n("Key: %1", QString::fromUtf8(currentDevice->verificationKey())));

    kcmUi->name_label->setText(currentDevice->name());
    setWhenAvailable(
        currentDevice->pairStateAsInt(),
        [this](int pairStateAsInt) {
            setCurrentDevicePairState(pairStateAsInt);
        },
        this);

    const QVector<KPluginMetaData> pluginInfo = KPluginMetaData::findPlugins(QStringLiteral("kdeconnect/"));
    QVector<KPluginMetaData> availablePluginInfo;

    m_oldSupportedPluginNames = currentDevice->supportedPlugins();
    for (auto it = pluginInfo.cbegin(), itEnd = pluginInfo.cend(); it != itEnd; ++it) {
        if (m_oldSupportedPluginNames.contains(it->pluginId())) {
            availablePluginInfo.append(*it);
        }
    }

    KSharedConfigPtr deviceConfig = KSharedConfig::openConfig(currentDevice->pluginsConfigFile());
    kcmUi->pluginSelector->clear();
    kcmUi->pluginSelector->setConfigurationArguments(QStringList(currentDevice->id()));
    kcmUi->pluginSelector->addPlugins(availablePluginInfo, i18n("Available plugins"));
    kcmUi->pluginSelector->setConfig(deviceConfig->group("Plugins"));
}

void KdeConnectKcm::requestPairing()
{
    if (!currentDevice) {
        return;
    }

    kcmUi->messages->hide();

    currentDevice->requestPairing();
}

void KdeConnectKcm::unpair()
{
    if (!currentDevice) {
        return;
    }

    currentDevice->unpair();
}

void KdeConnectKcm::acceptPairing()
{
    if (!currentDevice) {
        return;
    }

    currentDevice->acceptPairing();
}

void KdeConnectKcm::cancelPairing()
{
    if (!currentDevice) {
        return;
    }

    currentDevice->cancelPairing();
}

void KdeConnectKcm::pairingFailed(const QString &error)
{
    if (sender() != currentDevice)
        return;

    kcmUi->messages->setText(i18n("Error trying to pair: %1", error));
    kcmUi->messages->animatedShow();
}


void KdeConnectKcm::setCurrentDevicePairState(int pairStateAsInt)
{
    PairState state = (PairState)pairStateAsInt; // Hack because qdbus doesn't like enums
    kcmUi->accept_button->setVisible(state == PairState::RequestedByPeer);
    kcmUi->reject_button->setVisible(state == PairState::RequestedByPeer);
    kcmUi->cancel_button->setVisible(state == PairState::Requested);
    kcmUi->pair_button->setVisible(state == PairState::NotPaired);
    kcmUi->unpair_button->setVisible(state == PairState::Paired);
    kcmUi->progressBar->setVisible(state == PairState::Requested);
    kcmUi->ping_button->setVisible(state == PairState::Paired);
    switch (state) {
    case PairState::Paired:
        kcmUi->status_label->setText(i18n("(paired)"));
        break;
    case PairState::NotPaired:
        kcmUi->status_label->setText(i18n("(not paired)"));
        break;
    case PairState::RequestedByPeer:
        kcmUi->status_label->setText(i18n("(incoming pair request)"));
        break;
    case PairState::Requested:
        kcmUi->status_label->setText(i18n("(pairing requested)"));
        break;
    }
}

void KdeConnectKcm::pluginsConfigChanged(bool changed)
{
    if (!changed)
        return;

    if (!currentDevice)
        return;

    kcmUi->pluginSelector->save();
    currentDevice->reloadPlugins();
}

void KdeConnectKcm::save()
{
    KCModule::save();
}

void KdeConnectKcm::sendPing()
{
    if (!currentDevice)
        return;
    currentDevice->pluginCall(QStringLiteral("ping"), QStringLiteral("sendPing"));
}

QSize KdeConnectKcm::sizeHint() const
{
    return QSize(890, 550); // Golden ratio :D
}

QSize KdeConnectKcm::minimumSizeHint() const
{
    return QSize(500, 300);
}

#include "kcm.moc"
#include "moc_kcm.cpp"
