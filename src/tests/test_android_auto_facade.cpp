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

    auto emitVideoState(bool active) -> void { emit videoStateChanged(active); }

    auto emitVideoFrame(const QString& frameUrl, int width = 1280, int height = 720) -> void {
        emit videoFrameReceived(frameUrl, width, height);
    }

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

    void testVideoInactiveDebounceCancelsOnQuickFrameRecovery() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QVERIFY(m_mockCoreClient != nullptr);

        QSignalSpy frameSpy(&facade, &AndroidAutoFacade::projectionFrameUrlChanged);

        m_mockCoreClient->emitVideoFrame(QStringLiteral("data:image/jpeg;base64,first"), 800, 480);
        QTRY_COMPARE(facade.projectionFrameUrl(), QStringLiteral("data:image/jpeg;base64,first"));
        QCOMPARE(facade.projectionWidth(), 800);
        QCOMPARE(facade.projectionHeight(), 480);
        QVERIFY(facade.isVideoActive());

        m_mockCoreClient->emitVideoState(false);
        QTest::qWait(120);

        m_mockCoreClient->emitVideoFrame(QStringLiteral("data:image/jpeg;base64,second"), 800, 480);
        QTRY_COMPARE(facade.projectionFrameUrl(), QStringLiteral("data:image/jpeg;base64,second"));

        QTest::qWait(320);
        QVERIFY(facade.isVideoActive());
        QCOMPARE(facade.projectionFrameUrl(), QStringLiteral("data:image/jpeg;base64,second"));

        bool sawEmptyFrame = false;
        for (const auto& args : frameSpy) {
            const QString changedUrl = args.at(0).toString();
            if (changedUrl.isEmpty()) {
                sawEmptyFrame = true;
                break;
            }
        }
        QVERIFY(!sawEmptyFrame);
    }

    void testVideoInactiveDebounceClearsAfterTimeout() {
        auto& services = ServiceProvider::instance();
        AndroidAutoFacade facade(&services);

        QVERIFY(m_mockCoreClient != nullptr);

        m_mockCoreClient->emitVideoFrame(QStringLiteral("data:image/jpeg;base64,first"), 1024, 600);
        QTRY_VERIFY(facade.isVideoActive());
        QCOMPARE(facade.projectionFrameUrl(), QStringLiteral("data:image/jpeg;base64,first"));

        m_mockCoreClient->emitVideoState(false);
        QTest::qWait(450);

        QVERIFY(!facade.isVideoActive());
        QVERIFY(facade.projectionFrameUrl().isEmpty());
        QCOMPARE(facade.projectionWidth(), 0);
        QCOMPARE(facade.projectionHeight(), 0);
    }
};

QTEST_MAIN(AndroidAutoFacadeTest)
#include "test_android_auto_facade.moc"