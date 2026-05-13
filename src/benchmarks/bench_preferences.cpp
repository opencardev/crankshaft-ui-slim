#include <benchmark/benchmark.h>

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "PreferencesService.h"

class PreferencesFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State &) override {
        tmpDir = new QTemporaryDir();
        dbPath = tmpDir->path() + QStringLiteral("/bench-prefs.ini");
        service = new PreferencesService(dbPath);
        (void)service->initialize();

        // Pre-populate with representative settings
        (void)service->set(QStringLiteral("slim_ui.display.brightness"), 50);
        (void)service->set(QStringLiteral("slim_ui.audio.volume"), 75);
        (void)service->set(QStringLiteral("slim_ui.connection.preference"), QStringLiteral("USB"));
        (void)service->set(QStringLiteral("slim_ui.theme.mode"), QStringLiteral("DARK"));
        (void)service->set(QStringLiteral("slim_ui.device.lastConnected"), QStringLiteral("usb-001"));
    }

    void TearDown(benchmark::State &) override {
        delete service;
        delete tmpDir;
    }

protected:
    QTemporaryDir *tmpDir = nullptr;
    QString dbPath;
    PreferencesService *service = nullptr;
};

BENCHMARK_DEFINE_F(PreferencesFixture, CacheGet)(benchmark::State &state) {
    for (auto _ : state) {
        QVariant val = service->get(QStringLiteral("slim_ui.display.brightness"));
        benchmark::DoNotOptimize(val);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(PreferencesFixture, CacheGet);

BENCHMARK_DEFINE_F(PreferencesFixture, CacheGetWithDefault)(benchmark::State &state) {
    for (auto _ : state) {
        QVariant val = service->get(QStringLiteral("nonexistent.key"), 42);
        benchmark::DoNotOptimize(val);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(PreferencesFixture, CacheGetWithDefault);

BENCHMARK_DEFINE_F(PreferencesFixture, CacheContains)(benchmark::State &state) {
    for (auto _ : state) {
        bool found = service->contains(QStringLiteral("slim_ui.theme.mode"));
        benchmark::DoNotOptimize(found);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(PreferencesFixture, CacheContains);

BENCHMARK_DEFINE_F(PreferencesFixture, SetValue)(benchmark::State &state) {
    int i = 0;
    for (auto _ : state) {
        bool ok = service->set(QStringLiteral("slim_ui.display.brightness"), i % 101);
        benchmark::DoNotOptimize(ok);
        benchmark::ClobberMemory();
        ++i;
    }
}
BENCHMARK_REGISTER_F(PreferencesFixture, SetValue);

BENCHMARK_DEFINE_F(PreferencesFixture, AllKeys)(benchmark::State &state) {
    for (auto _ : state) {
        QStringList keys = service->allKeys();
        benchmark::DoNotOptimize(keys);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(PreferencesFixture, AllKeys);

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
