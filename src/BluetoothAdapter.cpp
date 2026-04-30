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

#include "BluetoothAdapter.h"

#include <QJsonObject>

#include "CoreClient.h"
#include "Logger.h"

BluetoothAdapter::BluetoothAdapter(CoreClient* coreClient, QObject* parent)
    : QObject(parent), m_coreClient(coreClient) {
    if (m_coreClient) {
        connect(m_coreClient, &CoreClient::bluetoothEventReceived, this,
                &BluetoothAdapter::onBluetoothEvent);
    }
    Logger::instance().debugContext("BluetoothAdapter", "Created (core WebSocket mode)");
}

BluetoothAdapter::~BluetoothAdapter() {
    Logger::instance().debugContext("BluetoothAdapter", "Destroyed");
}

bool BluetoothAdapter::isBtEnabled() const { return m_btEnabled; }
bool BluetoothAdapter::isDiscovering() const { return m_discovering; }
QString BluetoothAdapter::connectedAudioAddress() const { return m_connectedAudioAddress; }
QString BluetoothAdapter::connectedAudioName() const { return m_connectedAudioName; }
bool BluetoothAdapter::hasConnectedAudioDevice() const {
    return !m_connectedAudioAddress.isEmpty();
}
QString BluetoothAdapter::statusMessage() const { return m_statusMessage; }
QString BluetoothAdapter::lastError() const { return m_lastError; }
QVariantList BluetoothAdapter::getPairedAudioDevices() const { return m_pairedAudioDevices; }
QVariantList BluetoothAdapter::getDiscoveredAudioDevices() const {
    return m_discoveredAudioDevices;
}

void BluetoothAdapter::setBtEnabled(bool enabled) {
    if (!m_coreClient) return;
    QJsonObject payload;
    payload["enabled"] = enabled;
    m_coreClient->publish(QStringLiteral("bluetooth/enabled"), payload);
    setStatusMessage(enabled ? QStringLiteral("Enabling Bluetooth...")
                             : QStringLiteral("Disabling Bluetooth..."));
}

void BluetoothAdapter::startDiscovery() {
    if (!m_coreClient) return;
    m_coreClient->publish(QStringLiteral("bluetooth/scan/request"), QJsonObject());
    setStatusMessage(QStringLiteral("Scanning for Bluetooth devices..."));
}

void BluetoothAdapter::stopDiscovery() {
    if (!m_coreClient) return;
    m_coreClient->publish(QStringLiteral("bluetooth/scan/stop"), QJsonObject());
    setStatusMessage(QStringLiteral("Stopping scan..."));
}

void BluetoothAdapter::pairDevice(const QString& address) {
    if (!m_coreClient || address.isEmpty()) return;
    QJsonObject payload;
    payload["address"] = address;
    m_coreClient->publish(QStringLiteral("bluetooth/pair"), payload);
    setStatusMessage(QStringLiteral("Pairing with %1...").arg(address));
}

void BluetoothAdapter::unpairDevice(const QString& address) {
    if (!m_coreClient || address.isEmpty()) return;
    QJsonObject payload;
    payload["address"] = address;
    m_coreClient->publish(QStringLiteral("bluetooth/unpair"), payload);
    setStatusMessage(QStringLiteral("Unpairing %1...").arg(address));
}

void BluetoothAdapter::connectAudioDevice(const QString& address) {
    if (!m_coreClient || address.isEmpty()) return;
    QJsonObject payload;
    payload["address"] = address;
    m_coreClient->publish(QStringLiteral("bluetooth/connect"), payload);
    setStatusMessage(QStringLiteral("Connecting audio to %1...").arg(address));
}

void BluetoothAdapter::disconnectAudioDevice() {
    if (!m_coreClient) return;
    QJsonObject payload;
    if (!m_connectedAudioAddress.isEmpty()) {
        payload["address"] = m_connectedAudioAddress;
    }
    m_coreClient->publish(QStringLiteral("bluetooth/disconnect"), payload);
    setStatusMessage(QStringLiteral("Disconnecting audio..."));
}

void BluetoothAdapter::refreshDevices() {
    if (!m_coreClient) return;
    m_coreClient->publish(QStringLiteral("bluetooth/scan/request"), QJsonObject());
}

auto BluetoothAdapter::setStatusMessage(const QString& message) -> void {
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged(m_statusMessage);
}

auto BluetoothAdapter::setLastError(const QString& error) -> void {
    if (m_lastError == error) return;
    m_lastError = error;
    emit lastErrorChanged(m_lastError);
}

auto BluetoothAdapter::upsertDevice(const QVariantMap& device) -> void {
    const QString address = device.value("address").toString();
    if (address.isEmpty()) return;

    bool found = false;
    for (int i = 0; i < m_discoveredAudioDevices.size(); ++i) {
        QVariantMap existing = m_discoveredAudioDevices[i].toMap();
        if (existing.value("address").toString() == address) {
            // Merge: keep existing fields, override with new ones
            for (auto it = device.constBegin(); it != device.constEnd(); ++it) {
                existing.insert(it.key(), it.value());
            }
            m_discoveredAudioDevices[i] = existing;
            found = true;
            break;
        }
    }
    if (!found) {
        m_discoveredAudioDevices.append(device);
    }
    rebuildPairedList();
    emit devicesChanged();
}

auto BluetoothAdapter::removeDevice(const QString& address) -> void {
    for (int i = 0; i < m_discoveredAudioDevices.size(); ++i) {
        if (m_discoveredAudioDevices[i].toMap().value("address").toString() == address) {
            m_discoveredAudioDevices.removeAt(i);
            break;
        }
    }
    rebuildPairedList();
    emit devicesChanged();
}

auto BluetoothAdapter::rebuildPairedList() -> void {
    m_pairedAudioDevices.clear();
    for (const QVariant& v : m_discoveredAudioDevices) {
        if (v.toMap().value("paired").toBool()) {
            m_pairedAudioDevices.append(v);
        }
    }
}

void BluetoothAdapter::onBluetoothEvent(const QString& topic, const QVariantMap& payload) {
    if (topic == QLatin1String("bluetooth/enabled/result")) {
        const bool enabled = payload.value("enabled").toBool();
        if (m_btEnabled != enabled) {
            m_btEnabled = enabled;
            emit btEnabledChanged(m_btEnabled);
        }
        if (!enabled) {
            m_discovering = false;
            emit discoveringChanged(false);
        }
        const QString err = payload.value("error").toString();
        setLastError(err);
        setStatusMessage(enabled ? QStringLiteral("Bluetooth enabled")
                                 : QStringLiteral("Bluetooth disabled"));

    } else if (topic == QLatin1String("bluetooth/scan/result")) {
        const QVariantList devices = payload.value("devices").toList();
        if (!devices.isEmpty()) {
            m_discoveredAudioDevices.clear();
            for (const QVariant& deviceVar : devices) {
                upsertDevice(deviceVar.toMap());
            }
        } else {
            QVariantMap device = payload;
            upsertDevice(device);
            emit deviceDiscovered(device.value("name").toString(),
                                  device.value("address").toString());
        }
        if (!m_discovering) {
            m_discovering = true;
            emit discoveringChanged(true);
        }
        setStatusMessage(
            QStringLiteral("Discovered %1 device(s)").arg(m_discoveredAudioDevices.size()));

    } else if (topic == QLatin1String("bluetooth/scan/stopped")) {
        m_discovering = false;
        emit discoveringChanged(false);
        setStatusMessage(QStringLiteral("Bluetooth scan complete"));

    } else if (topic == QLatin1String("bluetooth/pair/result")) {
        const QString address = payload.value("address", payload.value("id")).toString();
        const bool success = payload.value("success").toBool();
        if (success) {
            for (int i = 0; i < m_discoveredAudioDevices.size(); ++i) {
                QVariantMap device = m_discoveredAudioDevices[i].toMap();
                if (device.value("address").toString() == address) {
                    device["paired"] = true;
                    m_discoveredAudioDevices[i] = device;
                    break;
                }
            }
            rebuildPairedList();
            emit devicesChanged();
            setLastError(QString());
            const QString rawName = payload.value("name").toString();
            const QString name = rawName.isEmpty() ? address : rawName;
            setStatusMessage(QStringLiteral("Paired with %1").arg(name));
            emit devicePaired(name, address);
        } else {
            setLastError(payload.value("error").toString());
            setStatusMessage(QStringLiteral("Pairing failed"));
        }

    } else if (topic == QLatin1String("bluetooth/connect/result")) {
        const QString address = payload.value("address", payload.value("id")).toString();
        const bool success = payload.value("success").toBool();
        const QString rawName = payload.value("name").toString();
        const QString name = rawName.isEmpty() ? address : rawName;
        if (success) {
            m_connectedAudioAddress = address;
            m_connectedAudioName = name;
            emit connectedAudioChanged();
            setLastError(QString());
            setStatusMessage(QStringLiteral("Audio connected: %1").arg(name));
            emit audioConnected(name, address);
        } else {
            setLastError(payload.value("error").toString());
            setStatusMessage(QStringLiteral("Audio connection failed"));
        }

    } else if (topic == QLatin1String("bluetooth/disconnect/result")) {
        const QString oldName = m_connectedAudioName;
        const QString oldAddress = m_connectedAudioAddress;
        m_connectedAudioAddress.clear();
        m_connectedAudioName.clear();
        emit connectedAudioChanged();
        setLastError(QString());
        setStatusMessage(QStringLiteral("Audio disconnected"));
        emit audioDisconnected(oldName, oldAddress);

    } else if (topic == QLatin1String("bluetooth/unpair/result")) {
        const QString address = payload.value("address", payload.value("id")).toString();
        const bool success = payload.value("success").toBool();
        if (success) {
            for (int i = 0; i < m_discoveredAudioDevices.size(); ++i) {
                QVariantMap device = m_discoveredAudioDevices[i].toMap();
                if (device.value("address").toString() == address) {
                    device["paired"] = false;
                    m_discoveredAudioDevices[i] = device;
                    break;
                }
            }
            rebuildPairedList();
            emit devicesChanged();
            const QString rawName = payload.value("name").toString();
            const QString name = rawName.isEmpty() ? address : rawName;
            setLastError(QString());
            setStatusMessage(QStringLiteral("Unpaired %1").arg(name));
            emit deviceUnpaired(name, address);
        } else {
            setLastError(payload.value("error").toString());
        }

    } else if (topic == QLatin1String("bluetooth/error")) {
        const QString err = payload.value("error").toString();
        setLastError(err.isEmpty() ? QStringLiteral("Bluetooth error") : err);
    }
}
