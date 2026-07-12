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

#include <QSignalSpy>
#include <QMetaObject>
#include <QtTest/QtTest>

#include "AndroidAutoFacade.h"
#include "AndroidAutoWebRtcSession.h"
#include "ConnectionStateMachine.h"
#include "CoreClient.h"
#include "ServiceProvider.h"
#include "TouchEventForwarder.h"

class MockCoreClient : public CoreClient {
    Q_OBJECT

public:
    explicit MockCoreClient(QObject* parent = nullptr) : CoreClient(parent) {}

    [[nodiscard]] auto initialize() -> bool override {
        initialized = true;
        emit connectionStateChanged(
            static_cast<int>(CoreClient::ConnectionState::Disconnected));
        return true;
    }

    auto shutdown() -> void override {
        emit audioStateChanged(false);
        emit videoStateChanged(false);
        emit connectedDeviceChanged(QString());
        emit connectionStateChanged(
            static_cast<int>(CoreClient::ConnectionState::Disconnected));
    }

    auto startSearching() -> void override {
        ++startSearchingCalls;
        emit connectionStateChanged(
            static_cast<int>(CoreClient::ConnectionState::Searching));
        QVariantMap mockDevice{{"deviceId", QStringLiteral("mock-device")},
                               {"name", QStringLiteral("Mock Phone")},
                               {"type", QStringLiteral("phone")},
                               {"signalStrength", 88}};
        emit deviceDiscovered(mockDevice);
    }

    auto stopSearching() -> void override {
        ++stopSearchingCalls;
        emit connectionStateChanged(
            static_cast<int>(CoreClient::ConnectionState::Disconnected));
    }

    auto connectToDevice(const QString& deviceId) -> void override {
        ++connectCalls;
        lastConnectedDeviceId = deviceId;

        if (forceErrorOnConnect || deviceId.isEmpty()) {
            emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Error));
            emit connectionError(QStringLiteral("Mock connect failure"));
            return;
        }

        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Connecting));
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Connected));
        emit connectedDeviceChanged(deviceId);
        emit projectionReadyChanged(true);
        emit videoStateChanged(true);
        emit audioStateChanged(true);
    }

    auto disconnect() -> void override {
        ++disconnectCalls;
        emit projectionReadyChanged(false);
        emit audioStateChanged(false);
        emit videoStateChanged(false);
        emit connectedDeviceChanged(QString());
        emit connectionStateChanged(
            static_cast<int>(CoreClient::ConnectionState::Disconnected));
    }

    auto sendTouchEvent(const QString& eventType, const QVariantList& points) -> void override {
        ++sendTouchEventCalls;
        lastTouchEventType = eventType;
        lastTouchPoints = points;
    }

    auto publish(const QString& topic, const QJsonObject& payload) -> void override {
        ++publishCalls;
        publishedTopics.append(topic);
        publishedPayloads.append(payload);
    }

    auto emitVideoTransportMode(const QString& mode) -> void {
        emit videoTransportModeChanged(mode);
    }

    auto emitWebRtcSignaling(const QString& topic, const QVariantMap& payload) -> void {
        emit webRtcSignalingReceived(topic, payload);
    }

    bool initialized{false};
    bool forceErrorOnConnect{false};

    int startSearchingCalls{0};
    int stopSearchingCalls{0};
    int connectCalls{0};
    int disconnectCalls{0};
    int sendTouchEventCalls{0};
    int publishCalls{0};

    QString lastConnectedDeviceId;
    QString lastTouchEventType;
    QVariantList lastTouchPoints;
    QList<QString> publishedTopics;
    QList<QJsonObject> publishedPayloads;
};

class CoreMockIntegrationTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        auto& services = ServiceProvider::instance();
        services.shutdown();
        QVERIFY(services.initialize());

        m_mockCoreClient = new MockCoreClient();
        services.injectCoreClientForTesting(std::unique_ptr<CoreClient>(m_mockCoreClient));
    }

    void cleanup() {
        ServiceProvider::instance().shutdown();
        m_mockCoreClient = nullptr;
    }

    void testAndroidAutoFacadeUsesMockedCoreClientForDiscoveryAndConnection() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QSignalSpy deviceSpy(&facade, &AndroidAutoFacade::deviceAdded);
        QSignalSpy stateSpy(&facade, &AndroidAutoFacade::connectionStateChanged);
        QSignalSpy connectedSpy(&facade, &AndroidAutoFacade::connectionEstablished);

        facade.startDiscovery();
        QTRY_COMPARE(m_mockCoreClient->startSearchingCalls, 1);
        QTRY_VERIFY(!deviceSpy.isEmpty());

        facade.connectToDevice(QStringLiteral("pixel-mock"));
        QTRY_COMPARE(m_mockCoreClient->connectCalls, 1);
        QTRY_VERIFY(!connectedSpy.isEmpty());
        QCOMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Connected));
        QCOMPARE(facade.connectedDeviceName(), QStringLiteral("pixel-mock"));
        QVERIFY(!stateSpy.isEmpty());
    }

    void testConnectionStateMachineRespondsToMockedCoreError() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        m_mockCoreClient->forceErrorOnConnect = true;

        QSignalSpy retrySpy(&machine, &ConnectionStateMachine::retryingChanged);
        machine.startConnection();
        facade.connectToDevice(QStringLiteral("unstable-device"));

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Error));
        QTRY_VERIFY(machine.isRetrying());
        QVERIFY(!retrySpy.isEmpty());
    }

    void testDisplayResolutionPublishingUsesScreenAwareScaling() {
        const QSize resolved = TouchEventForwarder::resolvePublishedDisplayResolution(
            QSize(1280, 720), QSize(1920, 1080), 1.5);

        QCOMPARE(resolved, QSize(2880, 1620));
    }

    void testTouchForwarderSendsEventsToMockedCoreClient() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        TouchEventForwarder forwarder(&facade, &services);

        QVariantList touchPoints;
        QVariantMap point;
        point["id"] = 1;
        point["x"] = 100.0;
        point["y"] = 120.0;
        point["pressure"] = 1.0;
        point["areaWidth"] = 8;
        point["areaHeight"] = 8;
        touchPoints.append(point);

        forwarder.forwardTouchEvent(QStringLiteral("press"), touchPoints);

        QTRY_COMPARE(m_mockCoreClient->sendTouchEventCalls, 1);
        QCOMPARE(m_mockCoreClient->lastTouchEventType, QStringLiteral("press"));
        QVERIFY(!m_mockCoreClient->lastTouchPoints.isEmpty());
    }

    void testRepeatedVideoFramesDoNotReemitActiveState() {
        CoreClient client;

        QSignalSpy videoStateSpy(&client, &CoreClient::videoStateChanged);
        QSignalSpy videoFrameSpy(&client, &CoreClient::videoFrameReceived);

        const QString firstMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/media/video-frame","payload":{"encoding":"jpeg-base64","data":"Zmlyc3Q=","width":1280,"height":720}})"
        );
        const QString secondMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/media/video-frame","payload":{"encoding":"jpeg-base64","data":"c2Vjb25k","width":1280,"height":720}})"
        );

        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, firstMessage)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, secondMessage)));

        QCOMPARE(videoFrameSpy.count(), 2);
        QCOMPARE(videoStateSpy.count(), 1);
        QCOMPARE(videoStateSpy.at(0).at(0).toBool(), true);
    }

    void testWebRtcEventsAndTransportModeAreExposed() {
        CoreClient client;

        QSignalSpy transportModeSpy(&client, &CoreClient::videoTransportModeChanged);
        QSignalSpy webRtcSpy(&client, &CoreClient::webRtcSignalingReceived);

        const QString channelStatusMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":true,"video_ready":false,"media_audio_ready":false,"video_transport_mode":"webrtc","video_transport_requested":"webrtc","video_transport_fallback_reason":"","reason":"webrtc_initialized"}})"
        );
        const QString offerMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/webrtc/offer","payload":{"type":"offer","sdp":"v=0"}})"
        );

        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, channelStatusMessage)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, offerMessage)));

        QCOMPARE(transportModeSpy.count(), 1);
        QCOMPARE(transportModeSpy.at(0).at(0).toString(), QStringLiteral("webrtc"));

        QCOMPARE(webRtcSpy.count(), 1);
        QCOMPARE(webRtcSpy.at(0).at(0).toString(), QStringLiteral("android-auto/webrtc/offer"));
        QCOMPARE(webRtcSpy.at(0).at(1).toMap().value(QStringLiteral("type")).toString(),
                 QStringLiteral("offer"));
    }

    void testWebRtcSessionTracksOfferAndPublishesAnswer() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        AndroidAutoWebRtcSession session(&facade);

        QSignalSpy activeSpy(&session, &AndroidAutoWebRtcSession::activeChanged);
        QSignalSpy offerSpy(&session, &AndroidAutoWebRtcSession::remoteOfferReceived);

        facade.connectToDevice(QStringLiteral("pixel-mock"));
        QTRY_COMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Connected));

        m_mockCoreClient->emitVideoTransportMode(QStringLiteral("webrtc"));
        QTRY_VERIFY(session.active());
        QVERIFY(!activeSpy.isEmpty());

        const QVariantMap offerPayload{{QStringLiteral("type"), QStringLiteral("offer")},
                                       {QStringLiteral("sdp"), QStringLiteral("v=0")}};
        m_mockCoreClient->emitWebRtcSignaling(QStringLiteral("android-auto/webrtc/offer"),
                                              offerPayload);

        QTRY_COMPARE(session.remoteOfferSdp(), QStringLiteral("v=0"));
        QCOMPARE(session.signalingState(), QStringLiteral("have-remote-offer"));
        QVERIFY(!offerSpy.isEmpty());

        session.sendAnswer(QStringLiteral("v=0\r\no=ui-slim-answer"));
        QTRY_COMPARE(m_mockCoreClient->publishCalls, 1);
        QCOMPARE(m_mockCoreClient->publishedTopics.last(), QStringLiteral("android-auto/webrtc/answer"));
        QCOMPARE(m_mockCoreClient->publishedPayloads.last().value(QStringLiteral("type")).toString(),
                 QStringLiteral("answer"));
        QCOMPARE(session.signalingState(), QStringLiteral("local-answer-sent"));

        session.sendIceCandidate(QStringLiteral("candidate:1 1 UDP 1 127.0.0.1 9 typ host"), 0,
                                 QStringLiteral("video"));
        QTRY_COMPARE(m_mockCoreClient->publishCalls, 2);
        QCOMPARE(m_mockCoreClient->publishedTopics.last(),
                 QStringLiteral("android-auto/webrtc/ice-candidate"));
    }

    void testChannelStatusIgnoresTransientFalseReadinessWhenConnected() {
        CoreClient client;

        QSignalSpy connectionSpy(&client, &CoreClient::connectionStateChanged);
        QSignalSpy videoStateSpy(&client, &CoreClient::videoStateChanged);
        QSignalSpy projectionSpy(&client, &CoreClient::projectionReadyChanged);

        const QString connectedMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":true,"video_ready":true,"media_audio_ready":true,"video_transport_mode":"webrtc","video_transport_requested":"webrtc","video_transport_fallback_reason":"","reason":"initial_ready"}})"
        );
        const QString transientFalseMessage = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":false,"video_ready":false,"media_audio_ready":true,"video_transport_mode":"webrtc","video_transport_requested":"webrtc","video_transport_fallback_reason":"","reason":"transient_reconfig"}})"
        );

        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, connectedMessage)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, transientFalseMessage)));

        QVERIFY(!connectionSpy.isEmpty());
        QCOMPARE(connectionSpy.last().at(0).toInt(),
                 static_cast<int>(CoreClient::ConnectionState::Connected));

        QCOMPARE(videoStateSpy.count(), 1);
        QCOMPARE(videoStateSpy.at(0).at(0).toBool(), true);

        QCOMPARE(projectionSpy.count(), 1);
        QCOMPARE(projectionSpy.at(0).at(0).toBool(), true);
    }

    void testChannelStatusDoesNotClearTransportModeOnTransientEmptyValue() {
        CoreClient client;

        QSignalSpy transportSpy(&client, &CoreClient::videoTransportModeChanged);

        const QString firstStatus = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":true,"video_ready":true,"media_audio_ready":true,"video_transport_mode":"webrtc","video_transport_requested":"webrtc","video_transport_fallback_reason":"","reason":"initialized"}})"
        );
        const QString secondStatus = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":true,"video_ready":true,"media_audio_ready":true,"video_transport_mode":"","video_transport_requested":"webrtc","video_transport_fallback_reason":"","reason":"transient_missing_mode"}})"
        );

        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, firstStatus)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, secondStatus)));

        QCOMPARE(transportSpy.count(), 1);
        QCOMPARE(transportSpy.at(0).at(0).toString(), QStringLiteral("webrtc"));
    }

    void testChannelStatusNormalizesTransportModeCase() {
        CoreClient client;

        QSignalSpy transportSpy(&client, &CoreClient::videoTransportModeChanged);

        const QString status = QStringLiteral(
            R"({"type":"event","topic":"android-auto/status/channel-status","payload":{"connection_state_name":"CONNECTED","projection_ready":true,"video_ready":true,"media_audio_ready":true,"video_transport_mode":"WebRTC","video_transport_requested":"WebRTC","video_transport_fallback_reason":"","reason":"case_variant"}})"
        );

        QVERIFY(QMetaObject::invokeMethod(&client, "onWebSocketTextReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, status)));

        QCOMPARE(transportSpy.count(), 1);
        QCOMPARE(transportSpy.at(0).at(0).toString(), QStringLiteral("webrtc"));
    }

private:
    MockCoreClient* m_mockCoreClient = nullptr;
};

QTEST_MAIN(CoreMockIntegrationTest)
#include "test_core_mock_integration.moc"
