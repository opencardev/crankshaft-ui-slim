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
#include "CoreClient.h"
#include "ServiceProvider.h"

class MockCoreClient : public CoreClient {
    Q_OBJECT

public:
    explicit MockCoreClient(QObject* parent = nullptr) : CoreClient(parent) {}

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
        QVariantMap mockDevice{{"deviceId", QStringLiteral("phone-1")},
                               {"name", QStringLiteral("Mock Phone")}};
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

class AndroidAutoFacadeTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        auto& services = ServiceProvider::instance();
        services.shutdown();
        QVERIFY(services.initialize());
        services.injectCoreClientForTesting(std::make_unique<MockCoreClient>());
    }

    void cleanup() { ServiceProvider::instance().shutdown(); }

    void testDiscoveryPublishesDevices() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QSignalSpy stateSpy(&facade, &AndroidAutoFacade::connectionStateChanged);
        QSignalSpy deviceSpy(&facade, &AndroidAutoFacade::deviceAdded);

        facade.startDiscovery();

        QTRY_VERIFY(!stateSpy.isEmpty());
        QTRY_VERIFY(!deviceSpy.isEmpty());
        QCOMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Searching));
    }

    void testConnectAndDisconnectFlow() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QSignalSpy connectedSpy(&facade, &AndroidAutoFacade::connectionEstablished);
        QSignalSpy disconnectedSpy(&facade, &AndroidAutoFacade::disconnectionRequested);

        facade.connectToDevice(QStringLiteral("phone-1"));

        QTRY_VERIFY(!connectedSpy.isEmpty());
        QCOMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Connected));
        QCOMPARE(facade.connectedDeviceName(), QStringLiteral("phone-1"));

        facade.disconnectDevice();

        QTRY_VERIFY(!disconnectedSpy.isEmpty());
        QCOMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Disconnected));
        QVERIFY(facade.connectedDeviceName().isEmpty());
    }

    void testConnectionErrorIsReported() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QSignalSpy errorSpy(&facade, &AndroidAutoFacade::connectionFailed);

        facade.connectToDevice(QString());

        QTRY_VERIFY(!errorSpy.isEmpty());
        QCOMPARE(facade.connectionState(), static_cast<int>(AndroidAutoFacade::Error));
        QVERIFY(!facade.lastError().isEmpty());
    }
};

QTEST_MAIN(AndroidAutoFacadeTest)
#include "test_android_auto_facade.moc"