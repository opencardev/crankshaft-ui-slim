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

#include "PreferencesService.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include "Logger.h"

PreferencesService::PreferencesService(const QString& dbPath, QObject* parent)
    : QObject(parent),
      m_dbPath(dbPath.isEmpty()
                   ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                         QStringLiteral("/slim-ui-preferences.ini")
                   : dbPath) {
    Logger::instance().infoContext(
        QStringLiteral("PreferencesService"),
        QStringLiteral("Initialised with storage file: %1").arg(m_dbPath));
}

PreferencesService::~PreferencesService() { delete m_settings; }

auto PreferencesService::initialize() -> bool {
    const QFileInfo fileInfo(m_dbPath);
    const QString directoryPath = fileInfo.absolutePath();
    if (!directoryPath.isEmpty()) {
        QDir directory(directoryPath);
        if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
            Logger::instance().errorContext(
                QStringLiteral("PreferencesService"),
                QStringLiteral("Failed to create preferences directory: %1").arg(directoryPath));
            return false;
        }
    }

    delete m_settings;
    m_settings = new QSettings(m_dbPath, QSettings::IniFormat, this);
    m_settings->sync();

    if (m_settings->status() != QSettings::NoError) {
        Logger::instance().errorContext(QStringLiteral("PreferencesService"),
                                        QStringLiteral("Failed to initialise preferences file"));
        return false;
    }

    m_cache.clear();
    const QStringList keys = m_settings->allKeys();
    for (const QString& key : keys) {
        m_cache.insert(key, m_settings->value(key));
    }

    Logger::instance().infoContext(QStringLiteral("PreferencesService"),
                                   QStringLiteral("Preferences storage initialised"));
    return true;
}

auto PreferencesService::get(const QString& key, const QVariant& defaultValue) const -> QVariant {
    if (m_cache.contains(key)) {
        return m_cache.value(key);
    }

    return defaultValue;
}

auto PreferencesService::set(const QString& key, const QVariant& value) -> bool {
    if (!m_settings) {
        return false;
    }

    m_settings->setValue(key, value);
    m_settings->sync();

    if (m_settings->status() != QSettings::NoError) {
        Logger::instance().errorContext(
            QStringLiteral("PreferencesService"),
            QStringLiteral("Failed to persist preference: %1").arg(key));
        return false;
    }

    m_cache.insert(key, value);
    emit preferenceChanged(key, value);
    return true;
}

auto PreferencesService::contains(const QString& key) const -> bool { return m_cache.contains(key); }

auto PreferencesService::remove(const QString& key) -> bool {
    if (!m_settings) {
        return false;
    }

    m_settings->remove(key);
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        return false;
    }

    m_cache.remove(key);
    return true;
}

auto PreferencesService::clear() -> bool {
    if (!m_settings) {
        return false;
    }

    m_settings->clear();
    m_settings->sync();

    if (m_settings->status() != QSettings::NoError) {
        return false;
    }

    m_cache.clear();
    return true;
}

auto PreferencesService::allKeys() const -> QStringList { return m_cache.keys(); }
