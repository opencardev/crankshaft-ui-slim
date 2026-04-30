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

#pragma once

#include <QObject>
#include <memory>

class CoreClient;
class PreferencesService;
class Logger;

/**
 * @brief Singleton provider for Crankshaft core services
 *
 * Manages lifecycle and access to core services used by the slim UI.
 * Initializes services in correct dependency order and provides
 * centralized access point for facade classes.
 */
class ServiceProvider : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     */
    static auto instance() -> ServiceProvider&;

    /**
     * @brief Initialize all slim UI services
     * @return true if initialization successful, false on error
     */
    [[nodiscard]] auto initialize() -> bool;

    /**
     * @brief Shutdown all core services
     */
    void shutdown();

    /**
     * @brief Check if services are initialized
     */
    [[nodiscard]] auto isInitialized() const -> bool { return m_initialized; }

    // Service accessors
    [[nodiscard]] auto androidAutoService() const -> CoreClient* { return m_coreClient.get(); }
    [[nodiscard]] auto preferencesService() const -> PreferencesService* {
        return m_preferencesService.get();
    }
    [[nodiscard]] auto logger() const -> Logger*;

    /**
     * @brief Test seam to replace the CoreClient with a mock/fake implementation.
     *
     * Intended for unit tests that need deterministic core behaviour.
     */
    void injectCoreClientForTesting(std::unique_ptr<CoreClient> coreClient);

signals:
    void initializationFailed(const QString& reason);
    void serviceReady();

private:
    ServiceProvider();
    ~ServiceProvider() override;
    ServiceProvider(const ServiceProvider&) = delete;
    ServiceProvider& operator=(const ServiceProvider&) = delete;

    bool initializePreferences();
    bool initializeCoreClient();

    std::unique_ptr<CoreClient> m_coreClient;
    std::unique_ptr<PreferencesService> m_preferencesService;

    bool m_initialized{false};
};