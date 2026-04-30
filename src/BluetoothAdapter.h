/*
 * Project: Crankshaft
 * This file is part of Crankshaft project.
 * Copyright (C) 2025 OpenCarDev Team
 *
 *  Crankshaft is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crankshaft is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crankshaft. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class CoreClient;

/**
 * @brief Bluetooth adapter for QML UI — routes commands through core WebSocket.
 *
 * All Bluetooth operations are delegated to crankshaft-core via CoreClient publish().
 * State updates are received from core via CoreClient::bluetoothEventReceived().
 */
class BluetoothAdapter : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool btEnabled READ isBtEnabled NOTIFY btEnabledChanged)
    Q_PROPERTY(bool discovering READ isDiscovering NOTIFY discoveringChanged)
    Q_PROPERTY(QVariantList pairedAudioDevices READ getPairedAudioDevices NOTIFY devicesChanged)
    Q_PROPERTY(
        QVariantList discoveredAudioDevices READ getDiscoveredAudioDevices NOTIFY devicesChanged)
    Q_PROPERTY(
        QString connectedAudioAddress READ connectedAudioAddress NOTIFY connectedAudioChanged)
    Q_PROPERTY(QString connectedAudioName READ connectedAudioName NOTIFY connectedAudioChanged)
    Q_PROPERTY(
        bool hasConnectedAudioDevice READ hasConnectedAudioDevice NOTIFY connectedAudioChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit BluetoothAdapter(CoreClient* coreClient, QObject* parent = nullptr);
    ~BluetoothAdapter() override;

    [[nodiscard]] auto isBtEnabled() const -> bool;
    [[nodiscard]] auto isDiscovering() const -> bool;
    [[nodiscard]] auto connectedAudioAddress() const -> QString;
    [[nodiscard]] auto connectedAudioName() const -> QString;
    [[nodiscard]] auto hasConnectedAudioDevice() const -> bool;
    [[nodiscard]] auto statusMessage() const -> QString;
    [[nodiscard]] auto lastError() const -> QString;

public slots:
    void setBtEnabled(bool enabled);
    void startDiscovery();
    void stopDiscovery();
    void pairDevice(const QString& address);
    void unpairDevice(const QString& address);
    void connectAudioDevice(const QString& address);
    void disconnectAudioDevice();
    void refreshDevices();
    Q_INVOKABLE auto getPairedAudioDevices() const -> QVariantList;
    Q_INVOKABLE auto getDiscoveredAudioDevices() const -> QVariantList;

signals:
    void btEnabledChanged(bool enabled);
    void discoveringChanged(bool discovering);
    void devicesChanged();
    void connectedAudioChanged();
    void statusMessageChanged(const QString& message);
    void lastErrorChanged(const QString& error);
    void deviceDiscovered(const QString& name, const QString& address);
    void devicePaired(const QString& name, const QString& address);
    void deviceUnpaired(const QString& name, const QString& address);
    void audioConnected(const QString& name, const QString& address);
    void audioDisconnected(const QString& name, const QString& address);

private slots:
    void onBluetoothEvent(const QString& topic, const QVariantMap& payload);

private:
    auto setStatusMessage(const QString& message) -> void;
    auto setLastError(const QString& error) -> void;
    auto upsertDevice(const QVariantMap& device) -> void;
    auto removeDevice(const QString& address) -> void;
    auto rebuildPairedList() -> void;

    CoreClient* m_coreClient{nullptr};
    bool m_btEnabled{false};
    bool m_discovering{false};
    QString m_connectedAudioAddress;
    QString m_connectedAudioName;
    QString m_statusMessage;
    QString m_lastError;
    QVariantList m_discoveredAudioDevices;
    QVariantList m_pairedAudioDevices;
};
