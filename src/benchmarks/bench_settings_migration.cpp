#include <benchmark/benchmark.h>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "PreferencesService.h"
#include "SettingsMigration.h"

class SettingsMigrationFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State &) override {
        tmpDir = new QTemporaryDir();
        dbPath = tmpDir->path() + QStringLiteral("/bench-migration.ini");
        service = new PreferencesService(dbPath);
        (void)service->initialize();
        migration = new SettingsMigration(service);

        // Set up complete valid settings
        (void)service->set(QStringLiteral("slim_ui.display.brightness"), 50);
        (void)service->set(QStringLiteral("slim_ui.audio.volume"), 75);
        (void)service->set(QStringLiteral("slim_ui.connection.preference"), QStringLiteral("USB"));
        (void)service->set(QStringLiteral("slim_ui.theme.mode"), QStringLiteral("DARK"));
        (void)service->set(QStringLiteral("slim_ui.device.lastConnected"), QStringLiteral("usb-001"));
        (void)service->set(QStringLiteral("slim_ui.schema.version"), 1);
    }

    void TearDown(benchmark::State &) override {
        delete migration;
        delete service;
        delete tmpDir;
    }

protected:
    QTemporaryDir *tmpDir = nullptr;
    QString dbPath;
    PreferencesService *service = nullptr;
    SettingsMigration *migration = nullptr;
};

BENCHMARK_DEFINE_F(SettingsMigrationFixture, DetectSchemaVersion)(benchmark::State &state) {
    for (auto _ : state) {
        int version = migration->detectSchemaVersion();
        benchmark::DoNotOptimize(version);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, DetectSchemaVersion);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, NeedsMigration)(benchmark::State &state) {
    for (auto _ : state) {
        bool needs = migration->needsMigration();
        benchmark::DoNotOptimize(needs);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, NeedsMigration);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, DetectCorruption)(benchmark::State &state) {
    for (auto _ : state) {
        bool corrupt = migration->detectCorruption();
        benchmark::DoNotOptimize(corrupt);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, DetectCorruption);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, ValidateBrightness)(benchmark::State &state) {
    for (auto _ : state) {
        bool valid = migration->validateSetting(QStringLiteral("slim_ui.display.brightness"), 75);
        benchmark::DoNotOptimize(valid);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, ValidateBrightness);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, ValidateThemeMode)(benchmark::State &state) {
    for (auto _ : state) {
        bool valid =
            migration->validateSetting(QStringLiteral("slim_ui.theme.mode"), QStringLiteral("DARK"));
        benchmark::DoNotOptimize(valid);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, ValidateThemeMode);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, ValidateConnectionPreference)(benchmark::State &state) {
    for (auto _ : state) {
        bool valid = migration->validateSetting(QStringLiteral("slim_ui.connection.preference"),
                                                QStringLiteral("USB"));
        benchmark::DoNotOptimize(valid);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, ValidateConnectionPreference);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, GetRequiredKeys)(benchmark::State &state) {
    for (auto _ : state) {
        QStringList keys = SettingsMigration::getRequiredSettingKeys();
        benchmark::DoNotOptimize(keys);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, GetRequiredKeys);

BENCHMARK_DEFINE_F(SettingsMigrationFixture, InitializeDefaults)(benchmark::State &state) {
    for (auto _ : state) {
        // All keys already exist, so this only checks containment
        bool ok = migration->initializeDefaults();
        benchmark::DoNotOptimize(ok);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(SettingsMigrationFixture, InitializeDefaults);

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
