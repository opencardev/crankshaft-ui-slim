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
#include "ServiceProvider.h"

class AudioFailureScenariosTest : public QObject {
    Q_OBJECT

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

    void testShutdownLeavesAudioUnavailable() {
        auto& services = ServiceProvider::instance();
        AudioBridge bridge(&services);

        bridge.shutdown();
        QVERIFY(!bridge.isAudioAvailable());
    }
};

QTEST_MAIN(AudioFailureScenariosTest)
#include "test_audio_failure_scenarios.moc"