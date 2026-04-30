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
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTimer>
#include <QSize>
#include <QVariantList>
#include <QVariantMap>
#include <QWebSocket>
#include <memory>

class CoreClient : public QObject {
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected = 0,
        Searching = 1,
        Connecting = 2,
        Connected = 3,
        Error = 4
    };

    explicit CoreClient(QObject* parent = nullptr);

    virtual ~CoreClient();

    [[nodiscard]] virtual auto initialize() -> bool;
    virtual auto shutdown() -> void;

    virtual auto startSearching() -> void;
    virtual auto stopSearching() -> void;
    virtual auto connectToDevice(const QString& deviceId) -> void;
    virtual auto disconnect() -> void;
    virtual auto sendTouchEvent(const QString& eventType, const QVariantList& points) -> void;
    virtual auto sendKeyEvent(const QString& keyName, const QString& action = QString(),
                              int keyCode = -1) -> void;
    virtual auto publish(const QString& topic, const QJsonObject& payload) -> void;

signals:
    void connectionStateChanged(int state);
    void bluetoothEventReceived(const QString& topic, const QVariantMap& payload);
    void deviceDiscovered(const QVariantMap& device);
    void deviceRemoved(const QString& deviceId);
    void connectedDeviceChanged(const QString& deviceName);
    void audioStateChanged(bool active);
    void videoStateChanged(bool active);
    void projectionReadyChanged(bool ready);
    void videoFrameReceived(const QString& frameUrl, int width, int height);
    void connectionError(const QString& error);

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextReceived(const QString& message);

private:
    auto setState(ConnectionState state) -> void;
    auto buildConnectionCandidates() -> void;
    auto openCurrentCandidate() -> void;
    auto advanceCandidate() -> void;
    auto connectToCore() -> void;
    auto subscribeToTopics() -> void;
    auto parseAndHandleEvent(const QJsonDocument& doc) -> void;

    ConnectionState m_state = ConnectionState::Disconnected;
    std::unique_ptr<QWebSocket> m_webSocket;
    QString m_coreUrl = QStringLiteral("ws://localhost:8080");
    QString m_primaryCoreUrl = QStringLiteral("ws://localhost:8080");
    QList<QString> m_connectionCandidates;
    int m_currentCandidateIndex = 0;
    int m_connectionAttempt = 0;
    bool m_isConnecting = false;
    bool m_shutdownRequested = false;
    bool m_hasConnected = false;
    bool m_reconnectScheduled = false;
    bool m_ignoreNextDisconnectFromAbort = false;
    bool m_hasLoggedFirstVideoFrame = false;
    bool m_projectionReady = false;
    bool m_videoReady = false;
    bool m_audioReady = false;
    QTimer* m_connectTimeoutTimer = nullptr;
};
