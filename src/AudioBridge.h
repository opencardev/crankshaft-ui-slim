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

#ifndef AUDIOBRIDGE_H
#define AUDIOBRIDGE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class CoreClient;
class ServiceProvider;

/**
 * @brief QML-facing control surface for core-owned audio.
 *
 * Core owns playback, master volume, mute, balance and ducking. This class holds
 * no audio state of its own: it sends `audio/*` commands to core and mirrors the
 * `audio/state` event that core sends on subscribe and after every change.
 */
class AudioBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isAudioAvailable READ isAudioAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString audioBackend READ audioBackend NOTIFY stateChanged)
    Q_PROPERTY(int volume READ volume NOTIFY stateChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY stateChanged)
    Q_PROPERTY(int balance READ balance NOTIFY stateChanged)
    Q_PROPERTY(bool duckingEnabled READ duckingEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool bluetoothAutoSelect READ bluetoothAutoSelect NOTIFY stateChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY stateChanged)
    Q_PROPERTY(QString selectedOutput READ selectedOutput NOTIFY stateChanged)
    Q_PROPERTY(QString activeOutput READ activeOutput NOTIFY stateChanged)

public:
    explicit AudioBridge(ServiceProvider* serviceProvider, QObject* parent = nullptr);
    ~AudioBridge() override = default;

    // Property getters
    [[nodiscard]] auto isAudioAvailable() const -> bool;
    [[nodiscard]] auto audioBackend() const -> QString;
    [[nodiscard]] auto volume() const -> int;
    [[nodiscard]] auto muted() const -> bool;
    [[nodiscard]] auto balance() const -> int;
    [[nodiscard]] auto duckingEnabled() const -> bool;
    // Switch to a Bluetooth output when its speaker connects. Core decides; this mirrors it.
    [[nodiscard]] auto bluetoothAutoSelect() const -> bool;
    // Each entry is a map with id, name and bluetooth. The list is core's current listing.
    [[nodiscard]] auto outputs() const -> QVariantList;
    // The chosen output id. Empty means the system default.
    [[nodiscard]] auto selectedOutput() const -> QString;
    // The output id in use. Empty means the system default.
    [[nodiscard]] auto activeOutput() const -> QString;

    // Q_INVOKABLE methods for QML
    // NOTE: Qt's MOC (Meta-Object Compiler) cannot handle 'auto' keyword in method
    // signatures. Explicit return types are required for Q_INVOKABLE methods.
    // NOLINTBEGIN(modernize-use-trailing-return-type)
    Q_INVOKABLE bool initialize();
    Q_INVOKABLE bool setVolume(int volume);  // 0-100
    Q_INVOKABLE bool setMuted(bool muted);
    Q_INVOKABLE bool setBalance(int balance);  // -100 (left) to 100 (right)
    Q_INVOKABLE bool setDuckingEnabled(bool enabled);
    Q_INVOKABLE bool setBluetoothAutoSelect(bool enabled);
    // Empty id selects the system default. Core rejects ids it does not list.
    Q_INVOKABLE bool setOutputDevice(const QString& id);
    // Sends the factory audio values to core: volume 50, unmuted, balance 0, ducking on.
    Q_INVOKABLE bool resetToDefaults();
    // NOLINTEND(modernize-use-trailing-return-type)

signals:
    void stateChanged();

private:
    auto applyState(const QVariantMap& state) -> void;

    CoreClient* m_coreClient = nullptr;
    bool m_isAudioAvailable = false;
    QString m_audioBackend;
    int m_volume = 50;
    bool m_muted = false;
    int m_balance = 0;
    bool m_duckingEnabled = true;
    bool m_bluetoothAutoSelect = true;
    QVariantList m_outputs;
    QString m_selectedOutput;
    QString m_activeOutput;
};

#endif  // AUDIOBRIDGE_H
