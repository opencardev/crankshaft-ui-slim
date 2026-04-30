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
#include <QtTest/QtTest>

#include "AndroidAutoFacade.h"
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

    bool initialized{false};
    bool forceErrorOnConnect{false};

    int startSearchingCalls{0};
    int stopSearchingCalls{0};
    int connectCalls{0};
    int disconnectCalls{0};
    int sendTouchEventCalls{0};

    QString lastConnectedDeviceId;
    QString lastTouchEventType;
    QVariantList lastTouchPoints;
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

private:
    MockCoreClient* m_mockCoreClient = nullptr;
};

QTEST_MAIN(CoreMockIntegrationTest)
#include "test_core_mock_integration.moc"
