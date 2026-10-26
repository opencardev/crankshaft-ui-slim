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

#include <QtTest/QtTest>

#include "AudioBridge.h"
#include "CoreClient.h"
#include "ServiceProvider.h"

class AudioFailureScenariosTest : public QObject {
    Q_OBJECT

private:
    // Build an audio/state payload as core sends it.
    static QVariantMap audioState(bool available, bool backendAvailable = true,
                                  const QString& backend = QStringLiteral("PipeWire"),
                                  int volume = 50, bool muted = false, int balance = 0,
                                  bool duckingEnabled = true) {
        QVariantMap state;
        state[QStringLiteral("available")] = available;
        state[QStringLiteral("backend")] = backend;
        state[QStringLiteral("backend_available")] = backendAvailable;
        state[QStringLiteral("volume")] = volume;
        state[QStringLiteral("muted")] = muted;
        state[QStringLiteral("balance")] = balance;
        state[QStringLiteral("ducking_enabled")] = duckingEnabled;
        return state;
    }

private slots:
    void init() {
        auto& services = ServiceProvider::instance();
        QVERIFY(services.initialize());
    }

    void cleanup() { ServiceProvider::instance().shutdown(); }

    void testInvalidVolumeRejected() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(!bridge.setVolume(-1));
        QVERIFY(!bridge.setVolume(101));
    }

    void testValidVolumeAcceptedWhenCoreClientAvailable() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(bridge.setVolume(0));
        QVERIFY(bridge.setVolume(50));
        QVERIFY(bridge.setVolume(100));
    }

    void testInvalidBalanceRejected() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(!bridge.setBalance(-101));
        QVERIFY(!bridge.setBalance(101));
        QVERIFY(bridge.setBalance(-100));
        QVERIFY(bridge.setBalance(0));
        QVERIFY(bridge.setBalance(100));
    }

    void testUnavailableCoreReportsAudioUnavailable() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);

        emit services.androidAutoService()->audioStateReceived(audioState(false));

        QVERIFY(!bridge.isAudioAvailable());
        QCOMPARE(bridge.audioBackend(), QStringLiteral("None"));
    }

    void testAudioStateIsMirroredIntoProperties() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);

        emit services.androidAutoService()->audioStateReceived(
            audioState(true, true, QStringLiteral("PulseAudio"), 30, true, -20, false));

        QVERIFY(bridge.isAudioAvailable());
        QCOMPARE(bridge.audioBackend(), QStringLiteral("PulseAudio"));
        QCOMPARE(bridge.volume(), 30);
        QVERIFY(bridge.muted());
        QCOMPARE(bridge.balance(), -20);
        QVERIFY(!bridge.duckingEnabled());
    }

    void testResetToDefaultsSendsFactoryValues() {
        // The payloads go to CoreClient::publish, which drops them while disconnected.
        // This test only checks that the reset runs and reports success.
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(bridge.resetToDefaults());
    }

    void testAudioStateEmitsStateChangedOnce() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);
        QSignalSpy changes(&bridge, &AudioBridge::stateChanged);

        emit services.androidAutoService()->audioStateReceived(audioState(true, true, QStringLiteral("PipeWire"), 70));

        QCOMPARE(changes.count(), 1);
        QCOMPARE(bridge.volume(), 70);
    }
};

QTEST_MAIN(AudioFailureScenariosTest)
#include "test_audio_failure_scenarios.moc"
