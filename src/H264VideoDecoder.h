/*
 * Project: Crankshaft
 * This file is part of Crankshaft project.
 * Copyright (C) 2026 OpenCarDev Team
 *
 * Licensed under the GNU General Public License, version 3 or later.
 */

#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QString>
#include <QHash>

#include <gst/gst.h>

class H264VideoDecoder final : public QObject {
    Q_OBJECT

public:
    explicit H264VideoDecoder(QObject* parent = nullptr);
    ~H264VideoDecoder() override;

    auto pushFrame(const QByteArray& data, int width, int height) -> void;
    auto stop() -> void;

signals:
    void frameReady(const QImage& image, int width, int height);
    void errorOccurred(const QString& error);

private:
    static auto onNewSample(GstElement* sink, gpointer userData) -> GstFlowReturn;
    static auto onDecodePadAdded(GstElement* decodebin, GstPad* pad, gpointer userData) -> void;
    static auto onPadProbe(GstPad* pad, GstPadProbeInfo* info, gpointer userData)
        -> GstPadProbeReturn;
    static auto onBusMessage(GstBus* bus, GstMessage* message, gpointer userData) -> gboolean;

    auto initialise() -> bool;
    auto buildPipeline(const char* decoderElementName, bool isHardwareDecoder) -> bool;
    auto teardownPipeline() -> void;
    auto shutdown() -> void;
    auto handleSample(GstSample* sample) -> GstFlowReturn;
    auto handleError(GError* error, const gchar* debug) -> void;

    GstElement* m_pipeline{nullptr};
    GstElement* m_appsrc{nullptr};
    GstElement* m_h264parse{nullptr};
    GstElement* m_decodebin{nullptr};
    GstElement* m_videoconvert{nullptr};
    GstElement* m_appsink{nullptr};
    GstBus* m_bus{nullptr};
    bool m_initialised{false};
    // True when m_decodebin is a hardware-accelerated V4L2 stateful decoder
    // (v4l2h264dec, backed by the Pi's VideoCore/HEVC decode block) rather
    // than the avdec_h264 software fallback. Logged for diagnostics and used
    // to decide whether a pipeline failure should retry with software decode.
    bool m_usingHardwareDecoder{false};
    // Set once a hardware-decoder attempt has failed (either at element
    // creation or at PLAYING state-change time) so we don't repeatedly retry
    // a decoder we already know is unusable on this device every time the
    // pipeline is rebuilt (e.g. on reconnect).
    bool m_hardwareDecoderUnavailable{false};
    int m_width{0};
    int m_height{0};
    quint64 m_pushedFrameCount{0};
    quint64 m_sampleCount{0};
    quint64 m_busWarningCount{0};
    quint64 m_busErrorCount{0};
    quint64 m_parserBufferCount{0};
    quint64 m_sourceBufferCount{0};
    quint64 m_parserInputBufferCount{0};
    quint64 m_decoderInputBufferCount{0};
    quint64 m_decoderOutputBufferCount{0};
    quint64 m_decoderTimingSampleCount{0};
    quint64 m_conversionTimingSampleCount{0};
    QHash<quint64, qint64> m_decoderStartTimesNs;
    QHash<QString, qint64> m_stageLastNs;
    QHash<QString, quint64> m_stageCounts;
    qint64 m_lastPushNs{0};
};
