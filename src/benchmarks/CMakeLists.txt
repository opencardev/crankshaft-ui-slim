cmake_minimum_required(VERSION 3.16)

include(FetchContent)

set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)

FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/CodSpeedHQ/codspeed-cpp
    SOURCE_SUBDIR google_benchmark
    GIT_TAG main
)

FetchContent_MakeAvailable(google_benchmark)

# Benchmark: Logger formatting and serialization
add_executable(bench_logger
    bench_logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logger.cpp
)
set_target_properties(bench_logger PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/benchmarks
)
target_link_libraries(bench_logger PRIVATE
    benchmark::benchmark
    Qt6::Core
)
target_include_directories(bench_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Benchmark: PreferencesService cache operations
add_executable(bench_preferences
    bench_preferences.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../PreferencesService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logger.cpp
)
set_target_properties(bench_preferences PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/benchmarks
)
target_link_libraries(bench_preferences PRIVATE
    benchmark::benchmark
    Qt6::Core
)
target_include_directories(bench_preferences PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Benchmark: SettingsMigration validation
add_executable(bench_settings_migration
    bench_settings_migration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SettingsMigration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../PreferencesService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logger.cpp
)
set_target_properties(bench_settings_migration PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/benchmarks
)
target_link_libraries(bench_settings_migration PRIVATE
    benchmark::benchmark
    Qt6::Core
)
target_include_directories(bench_settings_migration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Benchmark: ErrorHandler mapping and classification
add_executable(bench_error_handler
    bench_error_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ErrorHandler.cpp
)
set_target_properties(bench_error_handler PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ui-slim/benchmarks
)
target_link_libraries(bench_error_handler PRIVATE
    benchmark::benchmark
    Qt6::Core
)
target_include_directories(bench_error_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
