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

#include "CoreClient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QTimer>

#include "Logger.h"

namespace {
auto socketStateToString(QAbstractSocket::SocketState state) -> QString {
    switch (state) {
        case QAbstractSocket::UnconnectedState:
            return QStringLiteral("Unconnected");
        case QAbstractSocket::HostLookupState:
            return QStringLiteral("HostLookup");
        case QAbstractSocket::ConnectingState:
            return QStringLiteral("Connecting");
        case QAbstractSocket::ConnectedState:
            return QStringLiteral("Connected");
        case QAbstractSocket::BoundState:
            return QStringLiteral("Bound");
        case QAbstractSocket::ClosingState:
            return QStringLiteral("Closing");
        case QAbstractSocket::ListeningState:
            return QStringLiteral("Listening");
        default:
            return QStringLiteral("Unknown");
    }
}
}

CoreClient::CoreClient(QObject* parent)
        : QObject(parent),
            m_primaryCoreUrl(m_coreUrl),
            m_connectTimeoutTimer(new QTimer(this)) {}

CoreClient::~CoreClient() {
    if (m_webSocket) {
        m_webSocket->close();
    }
}

auto CoreClient::initialize() -> bool {
    Logger::instance().infoContext("CoreClient", "Initialising WebSocket core client");
    m_shutdownRequested = false;
    m_reconnectScheduled = false;
    m_hasConnected = false;

    m_connectTimeoutTimer->setSingleShot(true);
    connect(m_connectTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_shutdownRequested || (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState)) {
            return;
        }

        m_isConnecting = false;

        Logger::instance().warningContext(
            "CoreClient",
            QString("Connection attempt timed out for %1 (attempt=%2, candidate=%3/%4, socketState=%5, reconnectScheduled=%6)")
                .arg(m_connectionCandidates.value(m_currentCandidateIndex, m_coreUrl))
                .arg(m_connectionAttempt)
                .arg(m_currentCandidateIndex + 1)
                .arg(m_connectionCandidates.size())
                .arg(socketStateToString(m_webSocket ? m_webSocket->state() : QAbstractSocket::UnconnectedState))
                .arg(m_reconnectScheduled ? QStringLiteral("true") : QStringLiteral("false")));

        if (!m_reconnectScheduled) {
            m_reconnectScheduled = true;
            Logger::instance().infoContext(
                "CoreClient",
                QString("Scheduling reconnect after timeout in 500ms (candidate=%1/%2)")
                    .arg(m_currentCandidateIndex + 1)
                    .arg(m_connectionCandidates.size()));
            QTimer::singleShot(500, this, &CoreClient::connectToCore);
        }
    });

    buildConnectionCandidates();
    setState(ConnectionState::Disconnected);
    connectToCore();
    return true;
}

auto CoreClient::shutdown() -> void {
    m_shutdownRequested = true;
    m_reconnectScheduled = false;
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }
    setState(ConnectionState::Disconnected);
    if (m_webSocket) {
        m_webSocket->close();
    }
    emit connectedDeviceChanged(QString());
    emit audioStateChanged(false);
    emit videoStateChanged(false);
    m_projectionReady = false;
    m_videoReady = false;
    m_audioReady = false;
    emit projectionReadyChanged(false);
    Logger::instance().infoContext("CoreClient", "Shutdown WebSocket core client");
}

auto CoreClient::connectToCore() -> void {
    if (m_shutdownRequested) {
        return;
    }

    m_reconnectScheduled = false;

    if (!m_hasConnected && m_connectionAttempt > 0) {
        advanceCandidate();
    }

    if (m_isConnecting || m_state == ConnectionState::Connecting) {
        Logger::instance().debugContext(
            "CoreClient",
            QString("Already connecting to core (state=%1, isConnecting=%2, socketState=%3)")
                .arg(static_cast<int>(m_state))
                .arg(m_isConnecting ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(socketStateToString(m_webSocket ? m_webSocket->state() : QAbstractSocket::UnconnectedState)));
        return;
    }

    if (!m_webSocket) {
        m_webSocket = std::make_unique<QWebSocket>();

        connect(m_webSocket.get(), &QWebSocket::connected, this, &CoreClient::onWebSocketConnected);
        connect(m_webSocket.get(), &QWebSocket::disconnected, this,
                &CoreClient::onWebSocketDisconnected);
        // QWebSocket::errorOccurred was introduced in Qt 6.5. On older Qt 6 builds the
        // equivalent signal is the inherited QAbstractSocket::error, which requires an
        // explicit QOverload cast because it is an overloaded function.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(m_webSocket.get(), &QWebSocket::errorOccurred,
                this, &CoreClient::onWebSocketError);
#else
        connect(m_webSocket.get(), QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                this, &CoreClient::onWebSocketError);
#endif
        connect(m_webSocket.get(), &QWebSocket::textMessageReceived, this,
                &CoreClient::onWebSocketTextReceived);
    }

    openCurrentCandidate();
}

auto CoreClient::buildConnectionCandidates() -> void {
    m_connectionCandidates.clear();
    m_connectionCandidates.append(m_primaryCoreUrl);

    QUrl primaryUrl(m_primaryCoreUrl);
    if (primaryUrl.host().trimmed().toLower() == QStringLiteral("localhost")) {
        QUrl ipv4Url = primaryUrl;
        ipv4Url.setHost(QStringLiteral("127.0.0.1"));
        const QString ipv4 = ipv4Url.toString();
        if (ipv4 != m_primaryCoreUrl) {
            m_connectionCandidates.append(ipv4);
        }
    }

    m_currentCandidateIndex = 0;
}

auto CoreClient::openCurrentCandidate() -> void {
    if (m_connectionCandidates.isEmpty()) {
        buildConnectionCandidates();
    }

    const QString selectedUrl = m_connectionCandidates.value(m_currentCandidateIndex, m_coreUrl);
    m_coreUrl = selectedUrl;
    m_connectionAttempt++;
    m_isConnecting = true;

    Logger::instance().infoContext(
        "CoreClient",
        QString("Connecting to core WebSocket server at %1 (attempt=%2, candidate=%3/%4)")
            .arg(m_coreUrl)
            .arg(m_connectionAttempt)
            .arg(m_currentCandidateIndex + 1)
            .arg(m_connectionCandidates.size()));

    if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
        m_ignoreNextDisconnectFromAbort = true;
        Logger::instance().debugContext(
            "CoreClient",
            QString("Aborting existing socket before reconnect (state=%1)")
                .arg(socketStateToString(m_webSocket->state())));
        m_webSocket->abort();
    }
    m_webSocket->open(QUrl(m_coreUrl));
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->start(3000);
    }
}

auto CoreClient::advanceCandidate() -> void {
    if (m_connectionCandidates.size() <= 1) {
        return;
    }

    m_currentCandidateIndex = (m_currentCandidateIndex + 1) % m_connectionCandidates.size();
}

auto CoreClient::subscribeToTopics() -> void {
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        Logger::instance().warningContext("CoreClient", "Cannot subscribe: WebSocket not connected");
        return;
    }

    // Subscribe to Android Auto status events from core
    // The WebSocketServer broadcasts: android-auto/status/state-changed, connected, disconnected, error
    QJsonObject subscription;
    subscription["type"] = "subscribe";
    subscription["topic"] = "android-auto/status/*";

    QJsonDocument doc(subscription);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));

    QJsonObject mediaSubscription;
    mediaSubscription["type"] = "subscribe";
    mediaSubscription["topic"] = "android-auto/media/*";
    m_webSocket->sendTextMessage(QJsonDocument(mediaSubscription).toJson(QJsonDocument::Compact));

    QJsonObject btSubscription;
    btSubscription["type"] = "subscribe";
    btSubscription["topic"] = "bluetooth/#";
    m_webSocket->sendTextMessage(QJsonDocument(btSubscription).toJson(QJsonDocument::Compact));

    Logger::instance().infoContext("CoreClient",
                                   "Subscribed to android-auto/status/*, android-auto/media/*, bluetooth/#");
}

auto CoreClient::publish(const QString& topic, const QJsonObject& payload) -> void {
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        Logger::instance().warningContext("CoreClient",
            QString("Cannot publish %1: WebSocket not connected").arg(topic));
        return;
    }
    QJsonObject msg;
    msg["type"] = "publish";
    msg["topic"] = topic;
    msg["payload"] = payload;
    m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

auto CoreClient::startSearching() -> void {
    setState(ConnectionState::Searching);

    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject launchMessage;
        launchMessage["type"] = "publish";
        launchMessage["topic"] = "android-auto/launch";
        launchMessage["payload"] = QJsonObject();
        m_webSocket->sendTextMessage(QJsonDocument(launchMessage).toJson(QJsonDocument::Compact));
    }

    Logger::instance().infoContext("CoreClient", "Starting device search");
}

auto CoreClient::stopSearching() -> void {
    if (m_state == ConnectionState::Searching) {
        setState(ConnectionState::Disconnected);
    }
}

auto CoreClient::connectToDevice(const QString& deviceId) -> void {
    if (deviceId.isEmpty()) {
        setState(ConnectionState::Error);
        emit connectionError(QStringLiteral("Device ID is required"));
        return;
    }

    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        setState(ConnectionState::Error);
        emit connectionError(QStringLiteral("Not connected to core"));
        return;
    }

    setState(ConnectionState::Connecting);
    Logger::instance().infoContext(
        "CoreClient",
        QString("Connect requested for device %1; waiting for core Android Auto state events")
            .arg(deviceId));
}

auto CoreClient::disconnect() -> void {
    emit audioStateChanged(false);
    emit videoStateChanged(false);
    emit projectionReadyChanged(false);
    emit connectedDeviceChanged(QString());
    m_projectionReady = false;
    m_videoReady = false;
    m_audioReady = false;
    setState(ConnectionState::Disconnected);

    emit videoFrameReceived(QString(), 0, 0);
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject disconnectMessage;
        disconnectMessage["type"] = "publish";
        disconnectMessage["topic"] = "android-auto/disconnect";
        disconnectMessage["payload"] = QJsonObject();
        m_webSocket->sendTextMessage(QJsonDocument(disconnectMessage).toJson(QJsonDocument::Compact));
    }
}

auto CoreClient::sendTouchEvent(const QString& eventType, const QVariantList& points) -> void {
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        Logger::instance().warningContext("CoreClient", "Cannot send touch event: not connected");
        return;
    }

    if (points.isEmpty()) {
        return;
    }

    const QVariantMap firstPoint = points.first().toMap();
    if (firstPoint.isEmpty()) {
        return;
    }

    QJsonObject touchPayload;
    touchPayload["x"] = firstPoint.value("x").toDouble();
    touchPayload["y"] = firstPoint.value("y").toDouble();
    touchPayload["action"] = eventType;

    QJsonObject touchMessage;
    touchMessage["type"] = "publish";
    touchMessage["topic"] = "android-auto/touch";
    touchMessage["payload"] = touchPayload;
    m_webSocket->sendTextMessage(QJsonDocument(touchMessage).toJson(QJsonDocument::Compact));
}

auto CoreClient::sendKeyEvent(const QString& keyName, const QString& action, int keyCode) -> void {
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        Logger::instance().warningContext("CoreClient", "Cannot send key event: not connected");
        return;
    }

    const QString normalizedKey = keyName.trimmed().toUpper();
    if (normalizedKey.isEmpty() && keyCode < 0) {
        return;
    }

    QJsonObject keyPayload;
    if (!normalizedKey.isEmpty()) {
        keyPayload["key"] = normalizedKey;
    }
    if (!action.isEmpty()) {
        keyPayload["action"] = action;
    }
    if (keyCode >= 0) {
        keyPayload["key_code"] = keyCode;
    }

    QJsonObject keyMessage;
    keyMessage["type"] = "publish";
    keyMessage["topic"] = "android-auto/key";
    keyMessage["payload"] = keyPayload;
    m_webSocket->sendTextMessage(QJsonDocument(keyMessage).toJson(QJsonDocument::Compact));
}

auto CoreClient::setState(ConnectionState state) -> void {
    if (m_state == state) {
        return;
    }

    m_state = state;
    emit connectionStateChanged(static_cast<int>(m_state));
}

void CoreClient::onWebSocketConnected() {
    m_ignoreNextDisconnectFromAbort = false;
    Logger::instance().infoContext("CoreClient", "Connected to core WebSocket server");
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }
    m_isConnecting = false;
    m_hasConnected = true;
    m_reconnectScheduled = false;
    subscribeToTopics();
}

void CoreClient::onWebSocketDisconnected() {
    if (m_ignoreNextDisconnectFromAbort) {
        m_ignoreNextDisconnectFromAbort = false;
        Logger::instance().debugContext(
            "CoreClient",
            "Ignoring disconnect emitted by local reconnect abort");
        return;
    }

    const QString socketState = socketStateToString(m_webSocket ? m_webSocket->state() : QAbstractSocket::UnconnectedState);
    const int closeCode = m_webSocket ? static_cast<int>(m_webSocket->closeCode()) : -1;
    const QString closeReason = m_webSocket ? m_webSocket->closeReason() : QStringLiteral("<no-socket>");
    const QString socketErrorString = m_webSocket ? m_webSocket->errorString() : QStringLiteral("<no-socket>");

    Logger::instance().warningContext(
        "CoreClient",
        QString("Disconnected from core WebSocket server (url=%1, attempt=%2, closeCode=%3, closeReason=%4, socketState=%5, qtError=%6)")
            .arg(m_coreUrl)
            .arg(m_connectionAttempt)
            .arg(closeCode)
            .arg(closeReason)
            .arg(socketState)
            .arg(socketErrorString));
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }
    m_isConnecting = false;
    m_projectionReady = false;
    m_videoReady = false;
    m_audioReady = false;
    emit projectionReadyChanged(false);
    emit videoStateChanged(false);
    emit audioStateChanged(false);

    if (!m_shutdownRequested && !m_reconnectScheduled) {
        m_reconnectScheduled = true;
        Logger::instance().infoContext(
            "CoreClient",
            QString("Scheduling reconnect after disconnect in 2000ms (candidate=%1/%2)")
                .arg(m_currentCandidateIndex + 1)
                .arg(m_connectionCandidates.size()));
        QTimer::singleShot(2000, this, &CoreClient::connectToCore);
    }
}

void CoreClient::onWebSocketError(QAbstractSocket::SocketError error) {
    m_ignoreNextDisconnectFromAbort = false;
    m_isConnecting = false;
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }
    m_projectionReady = false;
    m_videoReady = false;
    m_audioReady = false;
    emit projectionReadyChanged(false);
    emit videoStateChanged(false);
    emit audioStateChanged(false);

    QString errorMsg;
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
            errorMsg = "Core server connection refused";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorMsg = "Core server closed connection";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorMsg = "Core server host not found";
            break;
        case QAbstractSocket::SocketTimeoutError:
            errorMsg = "Core server connection timeout";
            break;
        default:
            errorMsg = QString("WebSocket error: %1").arg(error);
    }

    const QString socketErrorString = m_webSocket ? m_webSocket->errorString() : QStringLiteral("<no-socket>");
    Logger::instance().warningContext(
        "CoreClient",
        QString("%1 (errorCode=%2, url=%3, attempt=%4, socketState=%5, qtError=%6)")
            .arg(errorMsg)
            .arg(static_cast<int>(error))
            .arg(m_coreUrl)
            .arg(m_connectionAttempt)
            .arg(socketStateToString(m_webSocket ? m_webSocket->state() : QAbstractSocket::UnconnectedState))
            .arg(socketErrorString));

    if (!m_shutdownRequested && !m_reconnectScheduled) {
        m_reconnectScheduled = true;
        Logger::instance().infoContext(
            "CoreClient",
            QString("Scheduling reconnect after socket error in 2000ms (candidate=%1/%2)")
                .arg(m_currentCandidateIndex + 1)
                .arg(m_connectionCandidates.size()));
        QTimer::singleShot(2000, this, &CoreClient::connectToCore);
    }
}

void CoreClient::onWebSocketTextReceived(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        Logger::instance().warningContext("CoreClient", "Received invalid JSON from core");
        return;
    }

    parseAndHandleEvent(doc);
}

auto CoreClient::parseAndHandleEvent(const QJsonDocument& doc) -> void {
    QJsonObject obj = doc.object();
    QString msgType = obj.value("type").toString();

    if (msgType == "event") {
        QString topic = obj.value("topic").toString();
        QJsonObject payload = obj.value("payload").toObject();

        Logger::instance().debugContext("CoreClient", QString("Received event: %1").arg(topic));

        if (topic == "android-auto/status/state-changed") {
            const int coreState = payload.value("state").toInt();

            ConnectionState mappedState = ConnectionState::Disconnected;
            switch (coreState) {
                case 0:  // DISCONNECTED
                    mappedState = ConnectionState::Disconnected;
                    break;
                case 1:  // SEARCHING
                    mappedState = ConnectionState::Searching;
                    break;
                case 2:  // CONNECTING
                case 3:  // AUTHENTICATING
                case 4:  // SECURING
                    mappedState = ConnectionState::Connecting;
                    break;
                case 5:  // CONNECTED
                    mappedState = ConnectionState::Connected;
                    break;
                case 6:  // DISCONNECTING
                    mappedState = ConnectionState::Disconnected;
                    break;
                case 7:  // ERROR
                    mappedState = ConnectionState::Error;
                    break;
                default:
                    mappedState = ConnectionState::Error;
                    break;
            }

            setState(mappedState);

        } else if (topic == "android-auto/status/connected") {
            QJsonObject device = payload.value("device").toObject();
            QString serialNumber = device.value("serial_number").toString();
            emit connectedDeviceChanged(serialNumber);

        } else if (topic == "android-auto/status/disconnected") {
            m_projectionReady = false;
            m_videoReady = false;
            m_audioReady = false;
            emit connectedDeviceChanged(QString());
            emit videoStateChanged(false);
            emit audioStateChanged(false);
            emit projectionReadyChanged(false);
            emit videoFrameReceived(QString(), 0, 0);
            setState(ConnectionState::Disconnected);

        } else if (topic == "android-auto/status/error") {
            QString errorMsg = payload.value("error").toString("Unknown error");
            Logger::instance().errorContext("CoreClient", QString("Core error: %1").arg(errorMsg));
            m_projectionReady = false;
            m_videoReady = false;
            m_audioReady = false;
            emit projectionReadyChanged(false);
            emit videoStateChanged(false);
            emit audioStateChanged(false);
            emit connectionError(errorMsg);
            setState(ConnectionState::Error);
        } else if (topic == "android-auto/status/channel-status") {
            const bool newProjectionReady = payload.value("projection_ready").toBool(false);
            const bool newVideoReady = payload.value("video_ready").toBool(false);
            const bool newAudioReady = payload.value("media_audio_ready").toBool(false);
            const bool controlVersionReceived = payload.value("control_version_received").toBool(false);
            const bool serviceDiscoveryCompleted = payload.value("service_discovery_completed").toBool(false);
            const QString connectionStateName = payload.value("connection_state_name").toString();
            const QString reason = payload.value("reason").toString();
            const bool coreReportsConnected =
                connectionStateName.compare(QStringLiteral("CONNECTED"), Qt::CaseInsensitive) == 0;

            Logger::instance().debugContext(
                "CoreClient",
                QString("channel-status: state=%1 projection_ready=%2 video_ready=%3 media_audio_ready=%4 "
                        "control_version_received=%5 service_discovery_completed=%6 reason=%7")
                    .arg(connectionStateName)
                    .arg(newProjectionReady ? "true" : "false")
                    .arg(newVideoReady ? "true" : "false")
                    .arg(newAudioReady ? "true" : "false")
                    .arg(controlVersionReceived ? "true" : "false")
                    .arg(serviceDiscoveryCompleted ? "true" : "false")
                    .arg(reason));

            if (newVideoReady != m_videoReady) {
                if (m_state == ConnectionState::Connected && !newVideoReady && coreReportsConnected) {
                    Logger::instance().debugContext(
                        "CoreClient",
                        QString("Ignoring transient video_ready=false while connected (reason=%1)")
                            .arg(reason));
                } else {
                    m_videoReady = newVideoReady;
                    emit videoStateChanged(m_videoReady);
                }
            }

            if (newAudioReady != m_audioReady) {
                m_audioReady = newAudioReady;
                emit audioStateChanged(m_audioReady);
            }

            if (newProjectionReady != m_projectionReady) {
                if (m_state == ConnectionState::Connected && !newProjectionReady && coreReportsConnected) {
                    Logger::instance().debugContext(
                        "CoreClient",
                        QString("Ignoring transient projection_ready=false while connected (reason=%1)")
                            .arg(reason));
                } else {
                    m_projectionReady = newProjectionReady;
                    emit projectionReadyChanged(m_projectionReady);
                    if (m_projectionReady) {
                        setState(ConnectionState::Connected);
                    }
                }
            }

            if (coreReportsConnected && m_state != ConnectionState::Connected) {
                setState(ConnectionState::Connected);
            } else if (m_state == ConnectionState::Connected && !m_projectionReady) {
                Logger::instance().debugContext(
                    "CoreClient",
                    QString("Connected state without projection readiness; keeping Connected "
                            "(state=%1 control_version_received=%2 service_discovery_completed=%3)")
                        .arg(connectionStateName)
                        .arg(controlVersionReceived ? "true" : "false")
                        .arg(serviceDiscoveryCompleted ? "true" : "false"));
            }
        } else if (topic == "android-auto/media/video-frame") {
            const QString encoding = payload.value("encoding").toString();
            const QString encodedData = payload.value("data").toString();
            const int width = payload.value("width").toInt();
            const int height = payload.value("height").toInt();

            if (!encodedData.isEmpty() && encoding == "jpeg-base64") {
                if (!m_hasLoggedFirstVideoFrame) {
                    m_hasLoggedFirstVideoFrame = true;
                    Logger::instance().infoContext(
                        "CoreClient", "First android-auto/media/video-frame event received",
                        {{"encoding", encoding},
                         {"width", width},
                         {"height", height},
                         {"payload_size", encodedData.size()}});
                }

                const QString frameUrl = QStringLiteral("data:image/jpeg;base64,%1").arg(encodedData);
                emit videoFrameReceived(frameUrl, width, height);
                emit videoStateChanged(true);
            }
        } else if (topic.startsWith(QStringLiteral("bluetooth/"))) {
            emit bluetoothEventReceived(topic, payload.toVariantMap());
        }

    } else if (msgType == "error") {
        QString errorMsg = obj.value("message").toString("Unknown error");
        Logger::instance().errorContext("CoreClient", QString("Core error: %1").arg(errorMsg));
        emit connectionError(errorMsg);
    }
}
