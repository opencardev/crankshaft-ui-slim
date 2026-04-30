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
#include <QTemporaryDir>

#include "PreferencesFacade.h"
#include "ServiceProvider.h"

class PreferencesFacadeTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

private slots:
    void init() {
        QVERIFY(m_tempDir.isValid());
        qputenv("XDG_DATA_HOME", m_tempDir.path().toUtf8());
        auto& services = ServiceProvider::instance();
        QVERIFY(services.initialize());
    }

    void cleanup() { ServiceProvider::instance().shutdown(); }

    void testDefaultsAreLoaded() {
        auto& services = ServiceProvider::instance();
        PreferencesFacade facade(&services);

        QCOMPARE(facade.displayBrightness(), 50);
        QCOMPARE(facade.audioVolume(), 50);
        QCOMPARE(facade.connectionPreference(), QStringLiteral("USB"));
        QCOMPARE(facade.themeMode(), QStringLiteral("DARK"));
    }

    void testRangeClamping() {
        auto& services = ServiceProvider::instance();
        PreferencesFacade facade(&services);

        facade.setDisplayBrightness(120);
        facade.setAudioVolume(-10);

        QCOMPARE(facade.displayBrightness(), 100);
        QCOMPARE(facade.audioVolume(), 0);
    }

    void testPersistenceAcrossFacadeInstances() {
        auto& services = ServiceProvider::instance();

        {
            PreferencesFacade first(&services);
            first.setDisplayBrightness(77);
            first.setAudioVolume(63);
            first.setThemeMode(QStringLiteral("LIGHT"));
            first.saveSettings();
        }

        PreferencesFacade second(&services);
        second.loadSettings();

        QCOMPARE(second.displayBrightness(), 77);
        QCOMPARE(second.audioVolume(), 63);
        QCOMPARE(second.themeMode(), QStringLiteral("LIGHT"));
    }
};

QTEST_MAIN(PreferencesFacadeTest)
#include "test_preferences_facade.moc"