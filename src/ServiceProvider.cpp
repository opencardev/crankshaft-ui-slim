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

#include "ServiceProvider.h"

#include <QStandardPaths>

#include "CoreClient.h"
#include "Logger.h"
#include "PreferencesService.h"

ServiceProvider::ServiceProvider() : QObject(nullptr) {
    // Constructor - services initialized via initialize()
}

ServiceProvider::~ServiceProvider() { shutdown(); }

ServiceProvider& ServiceProvider::instance() {
    static ServiceProvider instance;
    return instance;
}

auto ServiceProvider::initialize() -> bool {
    if (m_initialized) {
        return true;
    }

    Logger::instance().infoContext("ServiceProvider",
                                   "Initializing slim UI service layer");

    // Initialize local services in dependency order
    if (!initializePreferences()) {
        emit initializationFailed("Failed to initialize PreferencesService");
        return false;
    }

    if (!initializeCoreClient()) {
        emit initializationFailed("Failed to initialize CoreClient");
        return false;
    }

    m_initialized = true;
    Logger::instance().infoContext("ServiceProvider",
                                   "Slim UI service layer initialized successfully");
    emit serviceReady();

    return true;
}

auto ServiceProvider::shutdown() -> void {
    if (!m_initialized) {
        return;
    }

    Logger::instance().infoContext("ServiceProvider", "Shutting down slim UI service layer");

    if (m_coreClient) {
        m_coreClient->shutdown();
    }

    m_coreClient.reset();
    m_preferencesService.reset();

    m_initialized = false;
}

Logger* ServiceProvider::logger() const { return &Logger::instance(); }

void ServiceProvider::injectCoreClientForTesting(std::unique_ptr<CoreClient> coreClient) {
    if (m_coreClient) {
        m_coreClient->shutdown();
    }

    m_coreClient = std::move(coreClient);

    if (m_coreClient && !m_coreClient->parent()) {
        m_coreClient->setParent(this);
    }

    if (m_initialized && m_coreClient) {
        m_coreClient->initialize();
    }
}

auto ServiceProvider::initializePreferences() -> bool {
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                     "/slim-ui-preferences.db";

    m_preferencesService = std::make_unique<PreferencesService>(dbPath);

    if (!m_preferencesService->initialize()) {
        Logger::instance().errorContext(
            "ServiceProvider", "Failed to initialize preferences database", {{"dbPath", dbPath}});
        return false;
    }

    Logger::instance().infoContext("ServiceProvider", "PreferencesService initialized",
                                   {{"dbPath", dbPath}});
    return true;
}

auto ServiceProvider::initializeCoreClient() -> bool {
    if (!m_coreClient) {
        m_coreClient = std::make_unique<CoreClient>(this);
    } else if (!m_coreClient->parent()) {
        m_coreClient->setParent(this);
    }

    if (!m_coreClient->initialize()) {
        Logger::instance().errorContext("ServiceProvider", "Failed to initialize CoreClient");
        return false;
    }

    Logger::instance().infoContext("ServiceProvider", "CoreClient initialized");
    return true;
}