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

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "PreferencesService.h"
#include "SettingsMigration.h"

class SlimSettingsPersistenceTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

private slots:
    void testPersistenceAcrossServiceInstances() {
        QVERIFY(m_tempDir.isValid());
        const QString path = m_tempDir.path() + QStringLiteral("/prefs.ini");

        {
            PreferencesService service(path);
            QVERIFY(service.initialize());
            QVERIFY(service.set(QStringLiteral("slim_ui.display.brightness"), 73));
            QVERIFY(service.set(QStringLiteral("slim_ui.theme.mode"), QStringLiteral("LIGHT")));
        }

        PreferencesService reloaded(path);
        QVERIFY(reloaded.initialize());
        QCOMPARE(reloaded.get(QStringLiteral("slim_ui.display.brightness")).toInt(), 73);
        QCOMPARE(reloaded.get(QStringLiteral("slim_ui.theme.mode")).toString(),
                 QStringLiteral("LIGHT"));
    }

    void testCreatesMissingParentDirectory() {
        QVERIFY(m_tempDir.isValid());
        const QString nestedPath = m_tempDir.path() + QStringLiteral("/nested/settings/prefs.ini");
        const QFileInfo fileInfo(nestedPath);
        QVERIFY(!fileInfo.dir().exists());

        PreferencesService service(nestedPath);
        QVERIFY(service.initialize());
        QVERIFY(service.set(QStringLiteral("slim_ui.test.touch"), true));

        QVERIFY(fileInfo.dir().exists());
        QVERIFY(QFileInfo::exists(nestedPath));
    }

    void testMigrationInitialisesDefaults() {
        QVERIFY(m_tempDir.isValid());
        const QString path = m_tempDir.path() + QStringLiteral("/migration.ini");

        PreferencesService service(path);
        QVERIFY(service.initialize());

        SettingsMigration migration(&service);
        QVERIFY(migration.initializeDefaults());

        QVERIFY(service.contains(QStringLiteral("slim_ui.display.brightness")));
        QVERIFY(service.contains(QStringLiteral("slim_ui.audio.volume")));
        QVERIFY(service.contains(QStringLiteral("slim_ui.theme.mode")));
        QCOMPARE(service.get(QStringLiteral("slim_ui.schema.version")).toInt(),
                 SettingsMigration::CURRENT_SCHEMA_VERSION);
    }
};

QTEST_MAIN(SlimSettingsPersistenceTest)
#include "test_slim_settings_persistence.moc"