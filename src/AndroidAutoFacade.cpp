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

#include "AndroidAutoFacade.h"

#include "CoreClient.h"
#include "Logger.h"
#include "ServiceProvider.h"

AndroidAutoFacade::AndroidAutoFacade(ServiceProvider* serviceProvider, QObject* parent)
    : QObject(parent),
      m_serviceProvider(serviceProvider),
      m_connectionState(ConnectionState::Disconnected),
      m_connectedDeviceName(),
      m_lastError(),
      m_isVideoActive(false),
      m_isAudioActive(false),
      m_isProjectionReady(false),
      m_projectionFrameUrl(),
      m_projectionWidth(0),
      m_projectionHeight(0) {
    if (!m_serviceProvider) {
        Logger::instance().errorContext("AndroidAutoFacade", "ServiceProvider is null");
        return;
    }

    setupEventBusConnections();

    Logger::instance().infoContext("AndroidAutoFacade", "Initialized successfully");
}

AndroidAutoFacade::~AndroidAutoFacade() {
    Logger::instance().infoContext("AndroidAutoFacade", "Shutting down");
}

// Property getters
auto AndroidAutoFacade::connectionState() const -> int { return m_connectionState; }

auto AndroidAutoFacade::connectedDeviceName() const -> QString { return m_connectedDeviceName; }

auto AndroidAutoFacade::lastError() const -> QString { return m_lastError; }

auto AndroidAutoFacade::isVideoActive() const -> bool { return m_isVideoActive; }

auto AndroidAutoFacade::isAudioActive() const -> bool { return m_isAudioActive; }

auto AndroidAutoFacade::isProjectionReady() const -> bool { return m_isProjectionReady; }

auto AndroidAutoFacade::projectionFrameUrl() const -> QString { return m_projectionFrameUrl; }

auto AndroidAutoFacade::projectionWidth() const -> int { return m_projectionWidth; }

auto AndroidAutoFacade::projectionHeight() const -> int { return m_projectionHeight; }

// Q_INVOKABLE methods
auto AndroidAutoFacade::startDiscovery() -> void {
    Logger::instance().infoContext("AndroidAutoFacade", "Starting device discovery");

    auto* aaService = m_serviceProvider->androidAutoService();
    if (!aaService) {
        reportError("AndroidAuto service not available");
        return;
    }

    updateConnectionState(ConnectionState::Searching);

    // Delegate to core client
    aaService->startSearching();
}

auto AndroidAutoFacade::stopDiscovery() -> void {
    Logger::instance().infoContext("AndroidAutoFacade", "Stopping device discovery");

    auto* aaService = m_serviceProvider->androidAutoService();
    if (!aaService) {
        return;
    }

    aaService->stopSearching();

    if (m_connectionState == ConnectionState::Searching) {
        updateConnectionState(ConnectionState::Disconnected);
    }
}

auto AndroidAutoFacade::connectToDevice(const QString& deviceId) -> void {
    Logger::instance().infoContext("AndroidAutoFacade",
                                   QString("Connecting to device: %1").arg(deviceId));

    auto* aaService = m_serviceProvider->androidAutoService();
    if (!aaService) {
        reportError("AndroidAuto service not available");
        return;
    }

    updateConnectionState(ConnectionState::Connecting);

    // Delegate to core client
    aaService->connectToDevice(deviceId);
}

auto AndroidAutoFacade::disconnectDevice() -> void {
    Logger::instance().infoContext("AndroidAutoFacade", "Disconnecting device");

    auto* aaService = m_serviceProvider->androidAutoService();
    if (!aaService) {
        return;
    }

    emit disconnectionRequested();

    aaService->disconnect();
    updateConnectionState(ConnectionState::Disconnected);

    m_connectedDeviceName.clear();
    emit connectedDeviceNameChanged(m_connectedDeviceName);
}

auto AndroidAutoFacade::retryConnection() -> void {
    Logger::instance().infoContext("AndroidAutoFacade", "Retrying connection");

    m_lastError.clear();
    emit lastErrorChanged(m_lastError);

    startDiscovery();
}

// EventBus slot handlers
auto AndroidAutoFacade::onCoreConnectionStateChanged(int state) -> void {
    Logger::instance().debugContext("AndroidAutoFacade",
                                    QString("Core connection state changed: %1").arg(state));

    updateConnectionState(state);

    if (state == ConnectionState::Connected) {
        if (m_connectedDeviceName.isEmpty()) {
            m_connectedDeviceName = QStringLiteral("Connected Device");
            emit connectedDeviceNameChanged(m_connectedDeviceName);
        }

        if (m_isProjectionReady) {
            emit connectionEstablished(m_connectedDeviceName);
        }
    }
}

auto AndroidAutoFacade::onCoreDeviceDiscovered(const QVariantMap& device) -> void {
    Logger::instance().debugContext(
        "AndroidAutoFacade", QString("Device discovered: %1").arg(device.value("name").toString()));

    emit deviceAdded(device);

    // Emit updated device list
    QVariantList deviceList;
    deviceList.append(device);
    emit devicesDetected(deviceList);
}

auto AndroidAutoFacade::onCoreDeviceRemoved(const QString& deviceId) -> void {
    Logger::instance().debugContext("AndroidAutoFacade",
                                    QString("Device removed: %1").arg(deviceId));

    emit deviceRemoved(deviceId);
}

auto AndroidAutoFacade::onCoreVideoStateChanged(bool active) -> void {
    Logger::instance().debugContext(
        "AndroidAutoFacade",
        QString("Video state changed: %1").arg(active ? "active" : "inactive"));

    if (!active &&
        (m_connectionState == ConnectionState::Connected || m_isProjectionReady)) {
        Logger::instance().debugContext(
            "AndroidAutoFacade",
            "Ignoring transient inactive video state while Android Auto session remains active");
        return;
    }

    if (m_isVideoActive != active) {
        m_isVideoActive = active;
        emit isVideoActiveChanged(m_isVideoActive);

        if (!m_isVideoActive) {
            m_projectionFrameUrl.clear();
            m_projectionWidth = 0;
            m_projectionHeight = 0;
            emit projectionFrameUrlChanged(m_projectionFrameUrl);
            emit projectionFrameChanged(m_projectionWidth, m_projectionHeight);
        }
    }
}

auto AndroidAutoFacade::onCoreVideoFrameReceived(const QString& frameUrl, int width, int height)
    -> void {
    const bool frameUrlChanged = (m_projectionFrameUrl != frameUrl);
    const bool frameSizeChanged = (m_projectionWidth != width || m_projectionHeight != height);

    m_projectionFrameUrl = frameUrl;
    m_projectionWidth = width;
    m_projectionHeight = height;

    if (frameUrlChanged) {
        emit projectionFrameUrlChanged(m_projectionFrameUrl);
    }
    if (frameSizeChanged) {
        emit projectionFrameChanged(m_projectionWidth, m_projectionHeight);
    }
}

auto AndroidAutoFacade::onCoreAudioStateChanged(bool active) -> void {
    Logger::instance().debugContext(
        "AndroidAutoFacade",
        QString("Audio state changed: %1").arg(active ? "active" : "inactive"));

    if (m_isAudioActive != active) {
        m_isAudioActive = active;
        emit isAudioActiveChanged(m_isAudioActive);
    }
}

auto AndroidAutoFacade::onCoreProjectionReadyChanged(bool ready) -> void {
    if (m_isProjectionReady != ready) {
        m_isProjectionReady = ready;
        emit isProjectionReadyChanged(m_isProjectionReady);

        if (m_isProjectionReady && m_connectionState == ConnectionState::Connected) {
            if (m_connectedDeviceName.isEmpty()) {
                m_connectedDeviceName = QStringLiteral("Connected Device");
                emit connectedDeviceNameChanged(m_connectedDeviceName);
            }
            emit connectionEstablished(m_connectedDeviceName);
        }
    }
}

auto AndroidAutoFacade::onCoreConnectionError(const QString& error) -> void {
    Logger::instance().errorContext("AndroidAutoFacade",
                                    QString("Connection error: %1").arg(error));

    reportError(error);
    emit connectionFailed(error);
}

// Private methods
auto AndroidAutoFacade::setupEventBusConnections() -> void {
        auto* coreClient = m_serviceProvider->androidAutoService();
        if (!coreClient) {
        Logger::instance().warningContext("AndroidAutoFacade", "Core client not available");
        return;
    }

        connect(coreClient, &CoreClient::connectionStateChanged, this,
            &AndroidAutoFacade::onCoreConnectionStateChanged);
        connect(coreClient, &CoreClient::deviceDiscovered, this,
            &AndroidAutoFacade::onCoreDeviceDiscovered);
        connect(coreClient, &CoreClient::deviceRemoved, this,
            &AndroidAutoFacade::onCoreDeviceRemoved);
        connect(coreClient, &CoreClient::videoStateChanged, this,
            &AndroidAutoFacade::onCoreVideoStateChanged);
        connect(coreClient, &CoreClient::videoFrameReceived, this,
            &AndroidAutoFacade::onCoreVideoFrameReceived);
        connect(coreClient, &CoreClient::audioStateChanged, this,
            &AndroidAutoFacade::onCoreAudioStateChanged);
        connect(coreClient, &CoreClient::projectionReadyChanged, this,
            &AndroidAutoFacade::onCoreProjectionReadyChanged);
        connect(coreClient, &CoreClient::connectionError, this,
            &AndroidAutoFacade::onCoreConnectionError);
        connect(coreClient, &CoreClient::connectedDeviceChanged, this,
            [this](const QString& deviceName) {
            m_connectedDeviceName = deviceName;
            emit connectedDeviceNameChanged(m_connectedDeviceName);
            });

        Logger::instance().debugContext("AndroidAutoFacade", "Core client connections set up");
}

auto AndroidAutoFacade::updateConnectionState(int newState) -> void {
    if (m_connectionState == ConnectionState::Connected &&
        newState == ConnectionState::Connecting) {
        Logger::instance().warningContext(
            "AndroidAutoFacade",
            "Ignoring regressive state update Connected -> Connecting without disconnect/error");
        return;
    }

    if (m_connectionState != newState) {
        m_connectionState = newState;
        emit connectionStateChanged(m_connectionState);

        Logger::instance().infoContext(
            "AndroidAutoFacade", QString("Connection state updated: %1").arg(m_connectionState));
    }
}

auto AndroidAutoFacade::reportError(const QString& errorMessage) -> void {
    m_lastError = errorMessage;
    emit lastErrorChanged(m_lastError);

    updateConnectionState(ConnectionState::Error);

    Logger::instance().errorContext("AndroidAutoFacade", errorMessage);
}