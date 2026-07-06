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

#include <algorithm>
#include <cstring>

QVideoSinkProjectionVideoRenderer::QVideoSinkProjectionVideoRenderer(QObject* parent)
    : ProjectionVideoRenderer(parent), m_videoSink(new QVideoSink(this)) {}

auto QVideoSinkProjectionVideoRenderer::surfaceObject() const -> QObject* {
    return m_videoSink;
}

auto QVideoSinkProjectionVideoRenderer::presentImage(const QImage& image) -> void {
    if (image.isNull()) {
        clear();
        return;
    }

    const QImage frameImage = image.format() == QImage::Format_RGBA8888
        ? image
        : image.convertToFormat(QImage::Format_RGBA8888);

    QVideoFrameFormat format(frameImage.size(), QVideoFrameFormat::Format_RGBA8888);
    QVideoFrame frame(format);
    if (!frame.isValid() || !frame.map(QVideoFrame::WriteOnly)) {
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

    m_videoSink->setVideoFrame(frame);
}

auto QVideoSinkProjectionVideoRenderer::clear() -> void {
    m_videoSink->setVideoFrame(QVideoFrame());
}