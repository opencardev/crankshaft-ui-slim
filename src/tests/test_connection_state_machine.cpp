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

class MockCoreClient : public CoreClient {
    Q_OBJECT

public:
    explicit MockCoreClient(QObject* parent = nullptr) : CoreClient(parent) {}

    auto emitConnectionState(CoreClient::ConnectionState state) -> void {
        emit connectionStateChanged(static_cast<int>(state));
    }

    auto emitConnectionError(const QString& reason) -> void { emit connectionError(reason); }

    [[nodiscard]] auto initialize() -> bool override {
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Disconnected));
        return true;
    }

    auto shutdown() -> void override {
        emit projectionReadyChanged(false);
        emit connectedDeviceChanged(QString());
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Disconnected));
    }

    auto startSearching() -> void override {
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Searching));
        QVariantMap mockDevice{{"deviceId", QStringLiteral("device-1")},
                               {"name", QStringLiteral("Mock Device")}};
        emit deviceDiscovered(mockDevice);
    }

    auto connectToDevice(const QString& deviceId) -> void override {
        if (deviceId.isEmpty()) {
            emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Error));
            emit connectionError(QStringLiteral("Device ID is required"));
            return;
        }
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Connecting));
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Connected));
        emit connectedDeviceChanged(deviceId);
        emit projectionReadyChanged(true);
    }

    auto disconnect() -> void override {
        emit projectionReadyChanged(false);
        emit connectedDeviceChanged(QString());
        emit connectionStateChanged(static_cast<int>(CoreClient::ConnectionState::Disconnected));
    }
};

class ConnectionStateMachineTest : public QObject {
    Q_OBJECT

    MockCoreClient* m_mockCoreClient = nullptr;

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

    void testSuccessfulConnectionPath() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-1"));

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));
        QCOMPARE(machine.retryCount(), 0);
        QCOMPARE(machine.lastError(), QString());
    }

    void testConnectedDropWithinGraceDoesNotError() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        QSignalSpy retrySpy(&machine, &ConnectionStateMachine::retryingChanged);
        QSignalSpy maxRetriesSpy(&machine, &ConnectionStateMachine::maxRetriesReached);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-1"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        QVERIFY(m_mockCoreClient != nullptr);
        m_mockCoreClient->emitConnectionState(CoreClient::ConnectionState::Disconnected);
        QTest::qWait(120);
        m_mockCoreClient->emitConnectionState(CoreClient::ConnectionState::Connecting);
        QTest::qWait(40);
        m_mockCoreClient->emitConnectionState(CoreClient::ConnectionState::Connected);

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));
        QCOMPARE(machine.lastError(), QString());
        QCOMPARE(maxRetriesSpy.count(), 0);
        QCOMPARE(machine.isRetrying(), false);
        QVERIFY(retrySpy.isEmpty());
    }

    void testConnectedDropBeyondGraceTriggersErrorAndRetry() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        QSignalSpy retrySpy(&machine, &ConnectionStateMachine::retryingChanged);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-1"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        QVERIFY(m_mockCoreClient != nullptr);
        m_mockCoreClient->emitConnectionState(CoreClient::ConnectionState::Disconnected);

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Error));
        QCOMPARE(machine.lastError(), QStringLiteral("Lost connection to crankshaft-core service"));
        QTRY_VERIFY(machine.isRetrying());
        QVERIFY(!retrySpy.isEmpty());
    }

    void testIntentionalStopIgnoresFacadeFailureSignal() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-3"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        machine.stopConnection();
        QVERIFY(m_mockCoreClient != nullptr);
        m_mockCoreClient->emitConnectionError(QStringLiteral("disconnect cleanup failure"));

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Disconnected));
        QCOMPARE(machine.lastError(), QString());
        QCOMPARE(machine.isRetrying(), false);
    }

    void testExplicitFailureOverridesGraceImmediately() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-1"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        QVERIFY(m_mockCoreClient != nullptr);
        m_mockCoreClient->emitConnectionState(CoreClient::ConnectionState::Disconnected);
        m_mockCoreClient->emitConnectionError(QStringLiteral("Core handshake failed"));

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Error));
        QCOMPARE(machine.lastError(), QStringLiteral("Core handshake failed"));
    }

    void testIntentionalStopBypassesLostCoreError() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-2"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        machine.stopConnection();

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Disconnected));
        QCOMPARE(machine.lastError(), QString());
        QCOMPARE(machine.isRetrying(), false);
    }

    void testErrorTriggersRetry() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        QSignalSpy retrySpy(&machine, &ConnectionStateMachine::retryingChanged);

        machine.startConnection();
        facade.connectToDevice(QString());

        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Error));
        QTRY_VERIFY(machine.isRetrying());
        QVERIFY(!retrySpy.isEmpty());
    }

    void testStopConnectionResetsState() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);
        ConnectionStateMachine machine(&facade);

        machine.startConnection();
        facade.connectToDevice(QStringLiteral("device-2"));
        QTRY_COMPARE(machine.currentState(), static_cast<int>(ConnectionStateMachine::Connected));

        machine.stopConnection();
        QTRY_COMPARE(machine.currentState(),
                     static_cast<int>(ConnectionStateMachine::Disconnected));
    }
};

QTEST_MAIN(ConnectionStateMachineTest)
#include "test_connection_state_machine.moc"