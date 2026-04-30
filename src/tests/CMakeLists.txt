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

# Find Qt6 Test framework and additional components
find_package(Qt6 REQUIRED COMPONENTS Test Core Multimedia)

set(SLIM_UI_TEST_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CoreClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../PreferencesService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ServiceProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../AndroidAutoFacade.cpp
)

function(add_slim_ui_test test_name)
    cmake_parse_arguments(TEST "" "SOURCE" "EXTRA_SOURCES" ${ARGN})

    add_executable(${test_name}
        ${TEST_SOURCE}
        ${SLIM_UI_TEST_COMMON_SOURCES}
        ${TEST_EXTRA_SOURCES}
    )

    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/tests
    )

    target_link_libraries(${test_name} PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Multimedia
        Qt6::Sql
        Qt6::WebSockets
    )

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/tests
    )
endfunction()

add_slim_ui_test(
    test_android_auto_facade
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_android_auto_facade.cpp
)

add_slim_ui_test(
    test_connection_state_machine
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_connection_state_machine.cpp
    EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../ConnectionStateMachine.cpp
)

add_slim_ui_test(
    test_preferences_facade
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_preferences_facade.cpp
    EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../PreferencesFacade.cpp
)

add_slim_ui_test(
    test_slim_settings_persistence
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_slim_settings_persistence.cpp
    EXTRA_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../SettingsMigration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../PreferencesFacade.cpp
)

add_slim_ui_test(
    test_audio_failure_scenarios
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_failure_scenarios.cpp
    EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../AudioBridge.cpp
)

add_slim_ui_test(
    test_architecture_decoupling
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_decoupling.cpp
)

add_slim_ui_test(
    test_core_mock_integration
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_core_mock_integration.cpp
    EXTRA_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../ConnectionStateMachine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../TouchEventForwarder.cpp
)
