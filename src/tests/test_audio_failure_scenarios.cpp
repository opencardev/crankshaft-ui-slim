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

    void testOutputListIsMirroredFromCore() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);

        QVariantMap wired;
        wired[QStringLiteral("id")] = QStringLiteral("alsa_output.stereo-fallback");
        wired[QStringLiteral("name")] = QStringLiteral("Built-in Audio");
        wired[QStringLiteral("bluetooth")] = false;
        QVariantMap speaker;
        speaker[QStringLiteral("id")] = QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1");
        speaker[QStringLiteral("name")] = QStringLiteral("Car Speaker");
        speaker[QStringLiteral("bluetooth")] = true;

        QVariantMap state = audioState(true);
        state[QStringLiteral("outputs")] = QVariantList{wired, speaker};
        state[QStringLiteral("output")] = QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1");
        state[QStringLiteral("active_output")] = QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1");
        emit services.androidAutoService()->audioStateReceived(state);

        QCOMPARE(bridge.outputs().size(), 2);
        QCOMPARE(bridge.outputs().at(1).toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Car Speaker"));
        QCOMPARE(bridge.selectedOutput(), QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1"));
        QCOMPARE(bridge.activeOutput(), QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1"));
    }

    void testOutputsWithoutAnIdAreDropped() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);

        QVariantMap nameless;
        nameless[QStringLiteral("name")] = QStringLiteral("No id");
        QVariantMap usable;
        usable[QStringLiteral("id")] = QStringLiteral("hdmi_output");
        usable[QStringLiteral("name")] = QStringLiteral("HDMI");

        QVariantMap state = audioState(true);
        state[QStringLiteral("outputs")] = QVariantList{nameless, usable};
        emit services.androidAutoService()->audioStateReceived(state);

        QCOMPARE(bridge.outputs().size(), 1);
        QCOMPARE(bridge.outputs().at(0).toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("hdmi_output"));
    }

    void testUnavailableCoreClearsOutputs() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(services.androidAutoService() != nullptr);

        QVariantMap usable;
        usable[QStringLiteral("id")] = QStringLiteral("hdmi_output");
        QVariantMap state = audioState(true);
        state[QStringLiteral("outputs")] = QVariantList{usable};
        state[QStringLiteral("output")] = QStringLiteral("hdmi_output");
        emit services.androidAutoService()->audioStateReceived(state);
        QCOMPARE(bridge.outputs().size(), 1);

        emit services.androidAutoService()->audioStateReceived(audioState(false));

        QVERIFY(bridge.outputs().isEmpty());
        QVERIFY(bridge.selectedOutput().isEmpty());
        QVERIFY(bridge.activeOutput().isEmpty());
    }

    void testSetOutputDeviceAcceptedWhenCoreClientAvailable() {
        // The payload goes to CoreClient::publish, which drops it while disconnected.
        // This test only checks that the request is accepted for sending.
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(bridge.setOutputDevice(QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1")));
        QVERIFY(bridge.setOutputDevice(QString()));
    }

    void testBluetoothAutoSelectIsMirroredFromCore() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);
        QVERIFY(bridge.bluetoothAutoSelect());  // on by default

        QVariantMap state = audioState(true);
        state[QStringLiteral("bluetooth_auto_select")] = false;
        emit services.androidAutoService()->audioStateReceived(state);
        QVERIFY(!bridge.bluetoothAutoSelect());

        // A state without the key keeps the last value rather than turning the switch on.
        emit services.androidAutoService()->audioStateReceived(audioState(true));
        QVERIFY(!bridge.bluetoothAutoSelect());
    }

    void testSetBluetoothAutoSelectAcceptedWhenCoreClientAvailable() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        QVERIFY(bridge.setBluetoothAutoSelect(false));
        QVERIFY(bridge.setBluetoothAutoSelect(true));
    }
};

QTEST_MAIN(AudioFailureScenariosTest)
#include "test_audio_failure_scenarios.moc"
