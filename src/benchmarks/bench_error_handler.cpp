#include <benchmark/benchmark.h>

#include <QCoreApplication>
#include <QString>

#include "ErrorHandler.h"

static void BM_ErrorCodeToString(benchmark::State &state) {
    ErrorHandler handler;
    for (auto _ : state) {
        handler.reportError(ErrorHandler::ErrorCode::ConnectionFailed);
        QString code = handler.lastError();
        benchmark::DoNotOptimize(code);
        handler.clearError();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ErrorCodeToString);

static void BM_ErrorMessageWithContext(benchmark::State &state) {
    ErrorHandler handler;
    for (auto _ : state) {
        handler.reportError(ErrorHandler::ErrorCode::ConnectionTimeout,
                            QStringLiteral("device usb-001 on port 5277"));
        QString msg = handler.lastErrorMessage();
        benchmark::DoNotOptimize(msg);
        handler.clearError();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ErrorMessageWithContext);

static void BM_ClearError(benchmark::State &state) {
    ErrorHandler handler;
    handler.reportError(ErrorHandler::ErrorCode::DeviceNotFound);
    for (auto _ : state) {
        handler.clearError();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ClearError);

static void BM_HasErrorCheck(benchmark::State &state) {
    ErrorHandler handler;
    handler.reportError(ErrorHandler::ErrorCode::AudioStreamFailed);
    for (auto _ : state) {
        bool has = handler.hasError();
        benchmark::DoNotOptimize(has);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_HasErrorCheck);

static void BM_ReportAllErrorCodes(benchmark::State &state) {
    ErrorHandler handler;
    const ErrorHandler::ErrorCode codes[] = {
        ErrorHandler::ErrorCode::ConnectionFailed,
        ErrorHandler::ErrorCode::ConnectionTimeout,
        ErrorHandler::ErrorCode::DeviceNotFound,
        ErrorHandler::ErrorCode::DeviceDisconnected,
        ErrorHandler::ErrorCode::AudioBackendUnavailable,
        ErrorHandler::ErrorCode::AudioStreamFailed,
        ErrorHandler::ErrorCode::AudioDeviceNotFound,
        ErrorHandler::ErrorCode::VideoStreamFailed,
        ErrorHandler::ErrorCode::VideoDecoderFailed,
        ErrorHandler::ErrorCode::SettingsCorrupted,
        ErrorHandler::ErrorCode::SettingsSaveFailed,
        ErrorHandler::ErrorCode::SettingsLoadFailed,
        ErrorHandler::ErrorCode::ServiceInitFailed,
        ErrorHandler::ErrorCode::ServiceCrash,
        ErrorHandler::ErrorCode::UnknownError,
    };
    constexpr int numCodes = sizeof(codes) / sizeof(codes[0]);
    int i = 0;

    for (auto _ : state) {
        handler.reportError(codes[i % numCodes]);
        benchmark::DoNotOptimize(handler.lastError());
        handler.clearError();
        benchmark::ClobberMemory();
        ++i;
    }
}
BENCHMARK(BM_ReportAllErrorCodes);

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
