#include <benchmark/benchmark.h>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include "Logger.h"

// cppcheck-suppress constParameterCallback
static void BM_LoggerFormatJsonEntry(benchmark::State &state) {
    QJsonObject entry{
        {QStringLiteral("component"), QStringLiteral("Benchmark")},
        {QStringLiteral("level"), QStringLiteral("INFO")},
        {QStringLiteral("message"), QStringLiteral("Test log message for benchmarking")},
        {QStringLiteral("timestamp"), QStringLiteral("2025-01-01T00:00:00Z")},
        {QStringLiteral("thread"), QStringLiteral("12345")},
    };

    for (auto _ : state) {
        QString line = QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact));
        benchmark::DoNotOptimize(line);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LoggerFormatJsonEntry);

// cppcheck-suppress constParameterCallback
static void BM_LoggerFormatWithMetadata(benchmark::State &state) {
    QJsonObject entry{
        {QStringLiteral("component"), QStringLiteral("ConnectionManager")},
        {QStringLiteral("level"), QStringLiteral("WARNING")},
        {QStringLiteral("message"), QStringLiteral("Connection timeout on device")},
        {QStringLiteral("timestamp"), QStringLiteral("2025-01-01T00:00:00Z")},
        {QStringLiteral("thread"), QStringLiteral("12345")},
    };

    QVariantMap metadata;
    metadata[QStringLiteral("deviceId")] = QStringLiteral("usb-001");
    metadata[QStringLiteral("retryCount")] = 3;
    metadata[QStringLiteral("timeout")] = 15000;

    for (auto _ : state) {
        QJsonObject combined = entry;
        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            combined.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        QString line = QString::fromUtf8(QJsonDocument(combined).toJson(QJsonDocument::Compact));
        benchmark::DoNotOptimize(line);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LoggerFormatWithMetadata);

// cppcheck-suppress constParameterCallback
static void BM_LoggerLevelFiltering(benchmark::State &state) {
    Logger &logger = Logger::instance();
    logger.setLevel(Logger::Level::Warning);

    for (auto _ : state) {
        // Debug messages should be filtered out (below Warning level)
        logger.debugContext(QStringLiteral("Bench"), QStringLiteral("filtered message"));
        benchmark::ClobberMemory();
    }

    logger.setLevel(Logger::Level::Info);
}
BENCHMARK(BM_LoggerLevelFiltering);

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
