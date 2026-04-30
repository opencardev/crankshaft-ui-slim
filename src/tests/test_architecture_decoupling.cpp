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
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtTest/QtTest>

class ArchitectureDecouplingTest : public QObject {
    Q_OBJECT

private slots:
    void testSlimUiSourcesDoNotIncludeCoreDirectly() {
        const QString sourceRoot =
            QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..");
        const QDir sourceDir(sourceRoot);
        QVERIFY(sourceDir.exists());

        const QFileInfoList files = sourceDir.entryInfoList(
            QStringList() << QStringLiteral("*.h") << QStringLiteral("*.cpp"), QDir::Files);
        QVERIFY(!files.isEmpty());

        for (const QFileInfo& fileInfo : files) {
            QFile file(fileInfo.absoluteFilePath());
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(fileInfo.absoluteFilePath()));
            const QString content = QString::fromUtf8(file.readAll());

            QVERIFY2(!content.contains(QStringLiteral("../../core/")),
                     qPrintable(QStringLiteral("Direct core include found in %1")
                                    .arg(fileInfo.fileName())));
        }
    }

    void testSlimUiCMakeDoesNotEmbedCoreSources() {
        const QString cmakePath =
            QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/../CMakeLists.txt");
        QFile file(cmakePath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(cmakePath));

        const QString content = QString::fromUtf8(file.readAll());

        QVERIFY(!content.contains(QStringLiteral("${CMAKE_SOURCE_DIR}/core/services/")));
        QVERIFY(!content.contains(QStringLiteral("${CMAKE_SOURCE_DIR}/core/hal/")));
        QVERIFY(!content.contains(QStringLiteral("target_link_libraries(crankshaft-ui-slim PRIVATE aasdk")));
        QVERIFY(!content.contains(QStringLiteral("protobuf::libprotobuf")));
    }
};

QTEST_MAIN(ArchitectureDecouplingTest)
#include "test_architecture_decoupling.moc"
