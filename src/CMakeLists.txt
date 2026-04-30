# Project: Crankshaft
# This file is part of Crankshaft project.
# Copyright (C) 2025 OpenCarDev Team
#
#  Crankshaft is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  Crankshaft is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Crankshaft. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)

project(crankshaft-ui-slim)

include(GNUInstallDirs)

option(BUILD_TESTS "Build ui-slim tests" ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cmake_modules to MODULE_PATH for DebPackageFilename
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake_modules")

# Architecture detection for Debian packaging
if(TARGET_ARCH STREQUAL "amd64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(TARGET_ARCH STREQUAL "armhf")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
elseif(TARGET_ARCH STREQUAL "arm64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
elseif(TARGET_ARCH STREQUAL "i386")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
else()
    # Auto-detect architecture for native builds
    execute_process(
        COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE DETECTED_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(DETECTED_ARCH)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${DETECTED_ARCH}")
        message(STATUS "Detected architecture: ${DETECTED_ARCH}")
    else()
        # Fallback to system processor
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
        else()
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "unknown")
        endif()
        message(STATUS "Fallback architecture detection: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    endif()
endif()

# Include DEB package filename generation module
include(DebPackageFilename)

# Enable AUTOMOC, AUTORCC, AUTOUIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Multimedia
    Sql
    Network
    WebSockets
    DBus
)

# Optional translation tooling (qt6-l10n-tools on Debian/Ubuntu).
# Keep the build working when LinguistTools is not installed.
find_package(Qt6 QUIET COMPONENTS LinguistTools)

# Add main executable with slim-ui service sources
qt_add_executable(crankshaft-ui-slim
    main.cpp
    SlimUiApplicationRunner.h
    SlimUiApplicationRunner.cpp
    ServiceProvider.h
    ServiceProvider.cpp
    AndroidAutoFacade.h
    AndroidAutoFacade.cpp
    PreferencesFacade.h
    PreferencesFacade.cpp
    SettingsMigration.h
    SettingsMigration.cpp
    DisplayBrightnessController.h
    DisplayBrightnessController.cpp
    AudioVolumeController.h
    AudioVolumeController.cpp
    DeviceManager.h
    DeviceManager.cpp
    AudioBridge.h
    AudioBridge.cpp
    TouchEventForwarder.h
    TouchEventForwarder.cpp
    BluetoothAdapter.h
    BluetoothAdapter.cpp
    ConnectionStateMachine.h
    ConnectionStateMachine.cpp
    ErrorHandler.h
    ErrorHandler.cpp
    Logger.h
    Logger.cpp
    PreferencesService.h
    PreferencesService.cpp
    CoreClient.h
    CoreClient.cpp
)
target_compile_definitions(crankshaft-ui-slim PRIVATE CRANKSHAFT_AASDK_OLD_API)

# Set runtime output directory
set_target_properties(crankshaft-ui-slim PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim
)

# Link libraries
target_link_libraries(crankshaft-ui-slim PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
    Qt6::DBus
)

# Include directories
target_include_directories(crankshaft-ui-slim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add QML resources (QML files)
qt_add_resources(crankshaft-ui-slim "qml"
    PREFIX "/qml"
    BASE qml
    FILES
        qml/main.qml
        qml/ApplicationController.qml
        qml/Theme.qml
        qml/ThemeManager.qml
        qml/DeviceSelectionDialog.qml
        qml/AAProjectionView.qml
        qml/ConnectionStatusView.qml
        qml/ReconnectionPrompt.qml
        qml/SettingsPanel.qml
        qml/ErrorDialog.qml
        qml/ViewNavigationController.qml
        qml/components/SettingsSlider.qml
        qml/components/SegmentedOptionButton.qml
        qml/components/ThemeToggle.qml
        qml/components/ConnectionPreferenceToggle.qml
        qml/components/FactoryResetButton.qml
        qml/components/SettingsCard.qml
        qml/components/IconFontManager.qml
        qml/settings/DisplaySettingsTab.qml
        qml/settings/AudioSettingsTab.qml
        qml/settings/ConnectionSettingsTab.qml
        qml/settings/BluetoothSettingsTab.qml
        qml/assets/icon-settings-material.svg
        qml/assets/icon-settings-material-dark.svg
)

# Translation support
set(TS_FILES
    translations/slim-ui_en_GB.ts
)

if(Qt6LinguistTools_FOUND)
    qt_add_translations(crankshaft-ui-slim
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
    )
else()
    message(STATUS "Qt6 LinguistTools not found; skipping translation compilation")
endif()

# Clang-format integration
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(clang-format-slim-ui
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i
            *.h *.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on slim-ui source files"
    )
endif()
# Install slim-ui executable
install(TARGETS crankshaft-ui-slim
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT ui-slim
)

# Install systemd service unit
install(FILES ${CMAKE_SOURCE_DIR}/packaging/ui-slim/crankshaft-ui-slim.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
  COMPONENT ui-slim
)
# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Debian packaging
set(CRANKSHAFT_VERSION "0.1.0" CACHE STRING "Crankshaft UI Slim package version")
set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_PACKAGE_NAME "crankshaft-ui-slim")
set(CPACK_PACKAGE_VENDOR "OpenCarDev")
set(CPACK_PACKAGE_CONTACT "OpenCarDev Team")
set(CPACK_PACKAGE_VERSION "${CRANKSHAFT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "crankshaft-core")
set(CPACK_COMPONENTS_ALL ui-slim)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/packaging/ui-slim/postinst;${CMAKE_SOURCE_DIR}/packaging/ui-slim/prerm"
)

# Configure DEB package filename using the DebPackageFilename module
configure_deb_filename(
    PACKAGE_NAME "${CPACK_PACKAGE_NAME}"
    VERSION "${CRANKSHAFT_VERSION}"
    ARCHITECTURE "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}"
)

set(CPACK_MONOLITHIC_INSTALL ON)
include(CPack)
