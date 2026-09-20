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

#include "QVideoSinkProjectionVideoRenderer.h"

#include <QVideoFrameFormat>
#include <QVideoFrame>

#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <QMetaObject>
#include <QThread>

QVideoSinkProjectionVideoRenderer::QVideoSinkProjectionVideoRenderer(QObject* parent)
    : ProjectionVideoRenderer(parent), m_videoSink(new QVideoSink(this)) {}

auto QVideoSinkProjectionVideoRenderer::surfaceObject() const -> QObject* {
    return m_videoSink;
}

auto QVideoSinkProjectionVideoRenderer::setVideoSink(QVideoSink* videoSink) -> void {
    if (videoSink) {
        m_videoSink = videoSink;
    }
}

auto QVideoSinkProjectionVideoRenderer::presentImage(const QImage& image) -> void {
    if (image.isNull()) {
        clear();
        return;
    }

    // QImage has no Format_BGRA8888. On little-endian ARM64,
    // Format_ARGB32 has BGRA byte layout in memory.
    const QImage frameImage = image.format() == QImage::Format_ARGB32
        ? image
        : image.convertToFormat(QImage::Format_ARGB32);

    QVideoFrameFormat format(frameImage.size(), QVideoFrameFormat::Format_BGRA8888);
    QVideoFrame frame(format);
    if (!frame.isValid()) {
        Logger::instance().errorContext("QVideoSinkProjectionVideoRenderer",
                                        "Constructed QVideoFrame is invalid",
                                        {{"width", frameImage.width()},
                                         {"height", frameImage.height()},
                                         {"format", "BGRA8888"}});
        clear();
        return;
    }

    if (!frame.map(QVideoFrame::WriteOnly)) {
        Logger::instance().errorContext("QVideoSinkProjectionVideoRenderer",
                                        "Failed to map QVideoFrame for writing");
        clear();
        return;
    }

    const int imageBytesPerLine = static_cast<int>(frameImage.bytesPerLine());
    const int bytesPerLine = std::min(frame.bytesPerLine(0), imageBytesPerLine);
    for (int y = 0; y < frameImage.height(); ++y) {
        std::memcpy(frame.bits(0) + (y * frame.bytesPerLine(0)), frameImage.constScanLine(y),
                    bytesPerLine);
    }
    frame.unmap();

    if (!m_videoSink) {
        Logger::instance().errorContext("QVideoSinkProjectionVideoRenderer",
                                        "No QVideoSink is attached");
        return;
    }

    if (!m_diagnosticTimer.isValid()) {
        m_diagnosticTimer.start();
    }

    ++m_presentCount;
    if (m_presentCount == 1 || (m_presentCount % 30) == 0) {
        const qint64 elapsedMs = m_diagnosticTimer.elapsed();
        const double fps = elapsedMs > 0
            ? (static_cast<double>(m_presentCount) * 1000.0 / static_cast<double>(elapsedMs))
            : 0.0;
        Logger::instance().infoContext(
            "QVideoSinkProjectionVideoRenderer",
            "H264 renderer presentImage rate",
            {{"count", static_cast<qulonglong>(m_presentCount)},
             {"elapsed_ms", elapsedMs},
             {"approx_fps", QString::number(fps, 'f', 2)},
             {"width", frameImage.width()},
             {"height", frameImage.height()},
             {"thread", QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()))}});
    }

    // Queue the update onto the QVideoSink QObject thread so the QML boundary
    // is explicitly crossed using Qt's event queue.
    QVideoSink* sink = m_videoSink;
    const qint64 enqueueMs = m_diagnosticTimer.elapsed();
    const quint64 enqueueCount = m_presentCount;
    const bool queued = QMetaObject::invokeMethod(
        sink,
        [this, sink, frame, enqueueMs, enqueueCount]() {
            sink->setVideoFrame(frame);

            ++m_queuedDeliveryCount;
            if (m_queuedDeliveryCount == 1 || (m_queuedDeliveryCount % 30) == 0) {
                const qint64 nowMs = m_diagnosticTimer.elapsed();
                const qint64 queueDelayMs = nowMs - enqueueMs;
                Logger::instance().infoContext(
                    "QVideoSinkProjectionVideoRenderer",
                    "QVideoSink queued delivery rate",
                    {{"delivery_count", static_cast<qulonglong>(m_queuedDeliveryCount)},
                     {"source_count", static_cast<qulonglong>(enqueueCount)},
                     {"elapsed_ms", nowMs},
                     {"queue_delay_ms", queueDelayMs},
                     {"sink_thread", QString::number(
                         reinterpret_cast<quintptr>(QThread::currentThreadId()))}});
            }

            const QVideoFrame current = sink->videoFrame();
            if (m_queuedDeliveryCount == 1 || (m_queuedDeliveryCount % 30) == 0) {
                Logger::instance().infoContext(
                    "QVideoSinkProjectionVideoRenderer",
                    "QVideoSink frame after queued setVideoFrame",
                    {{"sink", QString::number(reinterpret_cast<quintptr>(sink))},
                     {"current_valid", current.isValid()},
                     {"current_width", current.width()},
                     {"current_height", current.height()},
                     {"current_pixel_format", static_cast<int>(current.pixelFormat())}});
            }
        },
        Qt::QueuedConnection);

    if (m_presentCount == 1 || (m_presentCount % 30) == 0) {
        Logger::instance().infoContext(
            "QVideoSinkProjectionVideoRenderer",
            "QVideoFrame queued to QML QVideoSink",
            {{"count", static_cast<qulonglong>(m_presentCount)},
             {"sink", QString::number(reinterpret_cast<quintptr>(sink))},
             {"queued", queued},
             {"sink_thread", QString::number(reinterpret_cast<quintptr>(sink->thread()))},
             {"current_thread", QString::number(
                 reinterpret_cast<quintptr>(QThread::currentThread()))}});
    }

}

auto QVideoSinkProjectionVideoRenderer::clear() -> void {
    m_videoSink->setVideoFrame(QVideoFrame());
}