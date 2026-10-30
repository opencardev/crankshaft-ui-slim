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

#include "AudioBridge.h"

#include <QJsonObject>

#include "CoreClient.h"
#include "Logger.h"
#include "ServiceProvider.h"

namespace {

constexpr int kVolumeMin = 0;
constexpr int kVolumeMax = 100;
constexpr int kBalanceMin = -100;
constexpr int kBalanceMax = 100;

// Factory values. These match the defaults core applies when it has no stored state.
constexpr int kDefaultVolume = 50;
constexpr bool kDefaultMuted = false;
constexpr int kDefaultBalance = 0;
constexpr bool kDefaultDuckingEnabled = true;

// Keeps only the outputs that have an id, since the id is what the user selects.
QVariantList usableOutputs(const QVariantList& listed) {
    QVariantList outputs;
    for (const QVariant& entry : listed) {
        const QVariantMap output = entry.toMap();
        if (!output.value(QStringLiteral("id")).toString().isEmpty()) {
            outputs.append(output);
        }
    }
    return outputs;
}

}  // namespace

AudioBridge::AudioBridge(ServiceProvider* serviceProvider, QObject* parent)
    : QObject(parent),
      m_coreClient(serviceProvider ? serviceProvider->androidAutoService() : nullptr),
      m_audioBackend(QStringLiteral("None")) {
    if (!m_coreClient) {
        Logger::instance().errorContext("AudioBridge", "Core client not available; audio controls disabled");
        return;
    }

    connect(m_coreClient, &CoreClient::audioStateReceived, this, &AudioBridge::applyState);
}

// Property getters
auto AudioBridge::isAudioAvailable() const -> bool { return m_isAudioAvailable; }

auto AudioBridge::audioBackend() const -> QString { return m_audioBackend; }

auto AudioBridge::volume() const -> int { return m_volume; }

auto AudioBridge::muted() const -> bool { return m_muted; }

auto AudioBridge::balance() const -> int { return m_balance; }

auto AudioBridge::duckingEnabled() const -> bool { return m_duckingEnabled; }

auto AudioBridge::bluetoothAutoSelect() const -> bool { return m_bluetoothAutoSelect; }

auto AudioBridge::outputs() const -> QVariantList { return m_outputs; }

auto AudioBridge::selectedOutput() const -> QString { return m_selectedOutput; }

auto AudioBridge::activeOutput() const -> QString { return m_activeOutput; }

// Q_INVOKABLE methods
auto AudioBridge::initialize() -> bool {
    if (!m_coreClient) {
        return false;
    }

    // Availability and current values arrive as an audio/state event once the
    // WebSocket is subscribed. Until then the controls show defaults.
    Logger::instance().infoContext("AudioBridge", "Audio controls bound to core");
    return true;
}

auto AudioBridge::setVolume(int volume) -> bool {
    if (volume < kVolumeMin || volume > kVolumeMax) {
        Logger::instance().warningContext(
            "AudioBridge", QString("Invalid volume level: %1 (must be 0-100)").arg(volume));
        return false;
    }
    if (!m_coreClient) {
        return false;
    }

    QJsonObject payload;
    payload["volume"] = volume;
    m_coreClient->publish(QStringLiteral("audio/volume/set"), payload);
    return true;
}

auto AudioBridge::setMuted(bool muted) -> bool {
    if (!m_coreClient) {
        return false;
    }

    QJsonObject payload;
    payload["muted"] = muted;
    m_coreClient->publish(QStringLiteral("audio/mute/set"), payload);
    return true;
}

auto AudioBridge::setBalance(int balance) -> bool {
    if (balance < kBalanceMin || balance > kBalanceMax) {
        Logger::instance().warningContext(
            "AudioBridge", QString("Invalid balance: %1 (must be -100 to 100)").arg(balance));
        return false;
    }
    if (!m_coreClient) {
        return false;
    }

    QJsonObject payload;
    payload["balance"] = balance;
    m_coreClient->publish(QStringLiteral("audio/balance/set"), payload);
    return true;
}

auto AudioBridge::resetToDefaults() -> bool {
    if (!m_coreClient) {
        Logger::instance().warningContext("AudioBridge",
                                          "Audio not reset: core client is not available");
        return false;
    }

    // Core owns the audio state, so the reset goes through the same commands as the controls.
    // Each command produces an audio/state event, so the controls update as core applies them.
    setVolume(kDefaultVolume);
    setMuted(kDefaultMuted);
    setBalance(kDefaultBalance);
    setDuckingEnabled(kDefaultDuckingEnabled);
    Logger::instance().infoContext("AudioBridge", "Factory audio values sent to core");
    return true;
}

auto AudioBridge::setDuckingEnabled(bool enabled) -> bool {
    if (!m_coreClient) {
        return false;
    }

    QJsonObject payload;
    payload["enabled"] = enabled;
    m_coreClient->publish(QStringLiteral("audio/ducking/set"), payload);
    return true;
}

auto AudioBridge::setBluetoothAutoSelect(bool enabled) -> bool {
    if (!m_coreClient) {
        return false;
    }

    QJsonObject payload;
    payload["enabled"] = enabled;
    m_coreClient->publish(QStringLiteral("audio/bluetooth_auto_select/set"), payload);
    return true;
}

auto AudioBridge::setOutputDevice(const QString& id) -> bool {
    if (!m_coreClient) {
        return false;
    }

    // Core checks the id against its listing, so this only sends the request.
    QJsonObject payload;
    payload["device"] = id;
    m_coreClient->publish(QStringLiteral("audio/output/set"), payload);
    return true;
}

// Private methods
auto AudioBridge::applyState(const QVariantMap& state) -> void {
    const bool available = state.value(QStringLiteral("available"), false).toBool();
    if (!available) {
        // Core has no audio router (for example, mock mode). Keep the last values but mark unavailable.
        m_isAudioAvailable = false;
        m_audioBackend = QStringLiteral("None");
        m_outputs.clear();
        m_selectedOutput.clear();
        m_activeOutput.clear();
        emit stateChanged();
        return;
    }

    m_isAudioAvailable = state.value(QStringLiteral("backend_available"), false).toBool();
    m_audioBackend = state.value(QStringLiteral("backend"), QStringLiteral("None")).toString();
    m_volume = state.value(QStringLiteral("volume"), m_volume).toInt();
    m_muted = state.value(QStringLiteral("muted"), m_muted).toBool();
    m_balance = state.value(QStringLiteral("balance"), m_balance).toInt();
    m_duckingEnabled = state.value(QStringLiteral("ducking_enabled"), m_duckingEnabled).toBool();
    m_bluetoothAutoSelect =
        state.value(QStringLiteral("bluetooth_auto_select"), m_bluetoothAutoSelect).toBool();
    m_outputs = usableOutputs(state.value(QStringLiteral("outputs")).toList());
    m_selectedOutput = state.value(QStringLiteral("output")).toString();
    m_activeOutput = state.value(QStringLiteral("active_output")).toString();

    Logger::instance().debugContext(
        "AudioBridge",
        QString("Audio state: backend=%1 volume=%2 muted=%3 balance=%4 ducking=%5")
            .arg(m_audioBackend)
            .arg(m_volume)
            .arg(m_muted)
            .arg(m_balance)
            .arg(m_duckingEnabled));

    emit stateChanged();
}
