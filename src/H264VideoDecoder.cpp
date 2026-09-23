/*
 * Project: Crankshaft
 * This file is part of Crankshaft project.
 * Copyright (C) 2026 OpenCarDev Team
 *
 * Licensed under the GNU General Public License, version 3 or later.
 */

#include "H264VideoDecoder.h"

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <QMutexLocker>
#include <QFile>
#include <QElapsedTimer>
#include <QStringList>
#include <QVideoFrameFormat>

#include <cstring>

#include "Logger.h"

namespace {
constexpr guint kMaxBuffers = 2;
constexpr const char* kSoftwareDecoderElementName = "avdec_h264";

[[nodiscard]] qint64 monotonicNs() {
    static QElapsedTimer timer;
    static const bool started = (timer.start(), true);
    Q_UNUSED(started);
    return timer.nsecsElapsed();
}

[[nodiscard]] QString readPlatformModel() {
    QFile modelFile(QStringLiteral("/proc/device-tree/model"));
    if (!modelFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(modelFile.readAll()).trimmed();
}

[[nodiscard]] QString requestedDecoderMode() {
    const QString value = qEnvironmentVariable("SLIM_UI_H264_DECODER").trimmed().toLower();
    if (value == QStringLiteral("software") || value == QStringLiteral("hardware") ||
        value == QStringLiteral("auto")) {
        return value;
    }
    return QStringLiteral("auto");
}

[[nodiscard]] bool gstElementFactoryExists(const char* factoryName) {
    GstElementFactory* factory = gst_element_factory_find(factoryName);
    if (!factory) {
        return false;
    }
    gst_object_unref(factory);
    return true;
}
}

H264VideoDecoder::H264VideoDecoder(QObject* parent) : QObject(parent) {
    gst_init(nullptr, nullptr);
}

H264VideoDecoder::~H264VideoDecoder() {
    shutdown();
}

auto H264VideoDecoder::detectHardwareDecoder(QString* decoderName, QString* platformModel) -> bool {
    const QString model = readPlatformModel();
    if (platformModel) *platformModel = model;

    // Different boards expose different V4L2 stateful/stateless H.264
    // decoder elements (backed by the VideoCore/HEVC decode block via
    // bcm2835-codec). Prefer whichever is the more modern/capable one for
    // the detected board, but still accept the other if that's what the
    // running kernel/GStreamer stack actually provides.
    QStringList candidates;
    if (model.contains(QStringLiteral("Raspberry Pi 5"), Qt::CaseInsensitive)) {
        candidates = {QStringLiteral("v4l2slh264dec"), QStringLiteral("v4l2h264dec")};
    } else {
        candidates = {QStringLiteral("v4l2h264dec"), QStringLiteral("v4l2slh264dec")};
    }

    for (const QString& candidate : candidates) {
        if (gstElementFactoryExists(candidate.toUtf8().constData())) {
            if (decoderName) *decoderName = candidate;
            return true;
        }
    }
    return false;
}

auto H264VideoDecoder::selectDecoderName(QString* platformModel, QString* selectionMode) -> QString {
    const QString requested = requestedDecoderMode();
    if (selectionMode) *selectionMode = requested;

    const QString model = readPlatformModel();
    if (platformModel) *platformModel = model;

    if (requested == QStringLiteral("software")) {
        return QLatin1String(kSoftwareDecoderElementName);
    }

    // Raspberry Pi 3 commonly exposes the legacy VideoCore H.264 path rather
    // than a fully usable V4L2 stateful/stateless decoder: the element can
    // reach PLAYING but never actually produce a decoded sample, which a
    // simple "did the pipeline start" check can't catch. Don't select it
    // there in "auto" mode; SLIM_UI_H264_DECODER=hardware can still force it
    // for a board/kernel combination that's known to work.
    if (requested == QStringLiteral("auto") &&
        model.contains(QStringLiteral("Raspberry Pi 3"), Qt::CaseInsensitive)) {
        return QLatin1String(kSoftwareDecoderElementName);
    }

    QString hardwareName;
    const bool hardwareAvailable = detectHardwareDecoder(&hardwareName, platformModel);
    if (hardwareAvailable) {
        return hardwareName;
    }

    if (requested == QStringLiteral("hardware")) {
        Logger::instance().infoContext(
            "H264VideoDecoder",
            "Hardware H.264 decoder requested but no supported V4L2 decoder is available; "
            "falling back to software");
    }
    return QLatin1String(kSoftwareDecoderElementName);
}

auto H264VideoDecoder::initialise() -> bool {
    if (m_initialised) return true;

    const QString decoderName = selectDecoderName(&m_platformModel, &m_decoderSelectionMode);
    const bool wantsHardware = (decoderName != QLatin1String(kSoftwareDecoderElementName));

    Logger::instance().infoContext(
        "H264VideoDecoder", "H.264 decoder selection",
        {{"mode", m_decoderSelectionMode},
         {"decoder", decoderName},
         {"hardware_accelerated", wantsHardware},
         {"platform_model", m_platformModel}});

    // Prefer the hardware decoder unless it was ruled out above, or we've
    // already established on this run that it isn't usable (missing plugin,
    // or a previous PLAYING attempt failed) - this avoids re-probing and
    // re-failing it on every reconnect once we know software is the only
    // option.
    if (wantsHardware && !m_hardwareDecoderUnavailable) {
        m_decoderName = decoderName;
        Logger::instance().infoContext(
            "H264VideoDecoder", "Attempting hardware-accelerated H.264 decode",
            {{"decoder", m_decoderName}});
        if (buildPipeline(m_decoderName.toUtf8().constData(), /*isHardwareDecoder=*/true)) {
            return true;
        }

        Logger::instance().warningContext(
            "H264VideoDecoder",
            "Hardware decoder unavailable or failed to start; falling back to software decode",
            {{"decoder", m_decoderName}});
        m_hardwareDecoderUnavailable = true;
    }

    m_decoderName = QLatin1String(kSoftwareDecoderElementName);
    Logger::instance().infoContext(
        "H264VideoDecoder", "Using software H.264 decode",
        {{"decoder", m_decoderName}});
    return buildPipeline(kSoftwareDecoderElementName, /*isHardwareDecoder=*/false);
}

auto H264VideoDecoder::buildPipeline(const char* decoderElementName, bool isHardwareDecoder) -> bool {
    Logger::instance().infoContext("H264VideoDecoder", "Initialising GStreamer H.264 pipeline",
                                    {{"decoder", decoderElementName}});

    m_pipeline = gst_pipeline_new("crankshaft-h264-decoder");
    m_appsrc = gst_element_factory_make("appsrc", "h264-source");
    m_h264parse = gst_element_factory_make("h264parse", "h264-parse");
    m_decodebin = gst_element_factory_make(decoderElementName, "h264-decoder");
    m_videoconvert = gst_element_factory_make("videoconvert", "video-convert");
    m_appsink = gst_element_factory_make("appsink", "video-sink");

    if (!m_pipeline || !m_appsrc || !m_h264parse || !m_decodebin ||
        !m_videoconvert || !m_appsink) {
        Logger::instance().errorContext(
            "H264VideoDecoder", "Required GStreamer H.264 elements are unavailable",
            {{"decoder", decoderElementName},
             {"pipeline", m_pipeline != nullptr},
             {"appsrc", m_appsrc != nullptr},
             {"h264parse", m_h264parse != nullptr},
             {"decoder_element", m_decodebin != nullptr},
             {"videoconvert", m_videoconvert != nullptr},
             {"appsink", m_appsink != nullptr},
             {"platform_model", m_platformModel}});
        if (!isHardwareDecoder) {
            // cppcheck-suppress shadowFunction
            emit errorOccurred(QStringLiteral("Required GStreamer H.264 elements are unavailable"));
        }
        teardownPipeline();
        return false;
    }

    GstCaps* caps = gst_caps_new_simple(
        "video/x-h264",
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "nal",
        nullptr);
    g_object_set(G_OBJECT(m_appsrc),
                 "stream-type", GST_APP_STREAM_TYPE_STREAM,
                 "is-live", TRUE,
                 "block", FALSE,
                 "format", GST_FORMAT_TIME,
                 "do-timestamp", TRUE,
                 "caps", caps,
                 nullptr);
    gst_caps_unref(caps);

    g_object_set(G_OBJECT(m_h264parse), "config-interval", -1, nullptr);

    GstCaps* sinkCaps = gst_caps_new_simple("video/x-raw",
                                            "format", G_TYPE_STRING, "BGRA",
                                            nullptr);
    g_object_set(G_OBJECT(m_appsink),
                 "caps", sinkCaps,
                 "max-buffers", kMaxBuffers,
                 "drop", TRUE,
                 "emit-signals", TRUE,
                 "sync", FALSE,
                 nullptr);
    gst_caps_unref(sinkCaps);

    gst_bin_add_many(GST_BIN(m_pipeline), m_appsrc, m_h264parse, m_decodebin,
                     m_videoconvert, m_appsink, nullptr);

    if (!gst_element_link_many(m_appsrc, m_h264parse, m_decodebin, m_videoconvert,
                               m_appsink, nullptr)) {
        Logger::instance().errorContext("H264VideoDecoder", "Failed to link GStreamer H.264 pipeline",
                                         {{"decoder", decoderElementName}});
        if (!isHardwareDecoder) {
            // cppcheck-suppress shadowFunction
            emit errorOccurred(QStringLiteral("Failed to link GStreamer H.264 pipeline"));
        }
        teardownPipeline();
        return false;
    }

    const auto addProbe = [this](GstElement* element, const char* padName) {
        GstPad* pad = gst_element_get_static_pad(element, padName);
        if (!pad) {
            return;
        }
        const auto probeType = static_cast<GstPadProbeType>(
            GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM);
        gst_pad_add_probe(pad, probeType, &H264VideoDecoder::onPadProbe, this, nullptr);
        gst_object_unref(pad);
    };
    addProbe(m_appsrc, "src");
    addProbe(m_h264parse, "sink");
    addProbe(m_h264parse, "src");
    addProbe(m_decodebin, "sink");
    addProbe(m_decodebin, "src");

    g_signal_connect(m_appsink, "new-sample", G_CALLBACK(onNewSample), this);

    m_bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_signal_watch(m_bus);
    g_signal_connect(m_bus, "message", G_CALLBACK(onBusMessage), this);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        Logger::instance().errorContext("H264VideoDecoder", "Failed to set GStreamer pipeline to PLAYING",
                                         {{"decoder", decoderElementName}});
        if (!isHardwareDecoder) {
            // cppcheck-suppress shadowFunction
            emit errorOccurred(QStringLiteral("Failed to start GStreamer H.264 pipeline"));
        }
        teardownPipeline();
        return false;
    }

    m_usingHardwareDecoder = isHardwareDecoder;
    m_initialised = true;
    Logger::instance().infoContext(
        "H264VideoDecoder", "GStreamer H.264 pipeline is PLAYING",
        {{"input_caps", "video/x-h264,stream-format=byte-stream,alignment=nal"},
         {"decoder", decoderElementName},
         {"hardware_accelerated", isHardwareDecoder},
         {"platform_model", m_platformModel},
         {"output_caps", "video/x-raw,format=BGRA"}});
    return true;
}

auto H264VideoDecoder::pushFrame(const QByteArray& data, int width, int height) -> void {
    if (data.isEmpty()) return;
    if (!initialise()) return;

    m_width = width;
    m_height = height;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, data.size(), nullptr);
    if (!buffer) {
        // cppcheck-suppress shadowFunction
        emit errorOccurred(QStringLiteral("Failed to allocate H.264 buffer"));
        return;
    }

    GstMapInfo map{};
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        // cppcheck-suppress shadowFunction
        emit errorOccurred(QStringLiteral("Failed to map H.264 buffer"));
        return;
    }
    std::memcpy(map.data, data.constData(), static_cast<size_t>(data.size()));
    gst_buffer_unmap(buffer, &map);

    const qint64 pushNowNs = monotonicNs();
    const qint64 pushIntervalMs =
        m_lastPushNs > 0 ? (pushNowNs - m_lastPushNs) / 1000000 : -1;
    m_lastPushNs = pushNowNs;

    const GstFlowReturn result = gst_app_src_push_buffer(GST_APP_SRC(m_appsrc), buffer);
    ++m_pushedFrameCount;
    if (m_pushedFrameCount == 1 || (m_pushedFrameCount % 30) == 0) {
        Logger::instance().infoContext(
            "H264VideoDecoder", "H.264 appsrc push cadence",
            {{"count", static_cast<qulonglong>(m_pushedFrameCount)},
             {"interval_ms", pushIntervalMs},
             {"bytes", data.size()},
             {"flow", static_cast<int>(result)}});
    }
    if (m_pushedFrameCount == 1 || (m_pushedFrameCount % 120) == 0) {
        Logger::instance().debugContext(
            "H264VideoDecoder", "Pushed encoded H.264 buffer to appsrc",
            {{"count", static_cast<qulonglong>(m_pushedFrameCount)},
             {"bytes", data.size()},
             {"width", width},
             {"height", height},
             {"prefix_hex", QString::fromLatin1(data.left(16).toHex())},
             {"flow", static_cast<int>(result)}});
    }
    if (result != GST_FLOW_OK && result != GST_FLOW_FLUSHING) {
        // cppcheck-suppress shadowFunction
        emit errorOccurred(QStringLiteral("GStreamer appsrc push failed: %1").arg(result));
    }

    while (GstMessage* message = gst_bus_pop_filtered(
               m_bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING))) {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            handleError(error, debug);
            if (error) g_error_free(error);
            if (debug) g_free(debug);
        }
        gst_message_unref(message);
    }
}

auto H264VideoDecoder::onDecodePadAdded(GstElement*, GstPad* pad, gpointer userData) -> void {
    auto* self = static_cast<H264VideoDecoder*>(userData);
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }
    if (!caps) return;

    const GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(structure);
    if (g_str_has_prefix(name, "video/")) {
        GstPad* sinkPad = gst_element_get_static_pad(self->m_videoconvert, "sink");
        if (sinkPad && !gst_pad_is_linked(sinkPad)) {
            gst_pad_link(pad, sinkPad);
        }
        if (sinkPad) gst_object_unref(sinkPad);
    }
    gst_caps_unref(caps);
}

auto H264VideoDecoder::onNewSample(GstElement* sink, gpointer userData) -> GstFlowReturn {
    auto* self = static_cast<H264VideoDecoder*>(userData);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;
    const auto result = self->handleSample(sample);
    gst_sample_unref(sample);
    return result;
}

auto H264VideoDecoder::onPadProbe(GstPad* pad, GstPadProbeInfo* info, gpointer userData)
    -> GstPadProbeReturn {
    auto* self = static_cast<H264VideoDecoder*>(userData);
    GstObject* parent = GST_OBJECT_PARENT(pad);
    const QString elementName = parent ? QString::fromUtf8(GST_OBJECT_NAME(parent)) : QString();
    const QString padName = QString::fromUtf8(GST_PAD_NAME(pad));

    if ((GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) != 0) {
        GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
        if (event && GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
            GstCaps* caps = nullptr;
            gst_event_parse_caps(event, &caps);
            gchar* capsText = caps ? gst_caps_to_string(caps) : nullptr;
            Logger::instance().infoContext(
                "H264VideoDecoder", "GStreamer caps observed",
                {{"element", elementName},
                 {"pad", padName},
                 {"caps", capsText ? QString::fromUtf8(capsText) : QString()}});
            g_free(capsText);
        }
    }

    if ((GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER) != 0) {
        GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        quint64* count = nullptr;
        QString stage;
        if (elementName == QStringLiteral("h264-source")) {
            stage = QStringLiteral("appsrc-src");
        } else if (elementName == QStringLiteral("h264-parse") &&
                   padName == QStringLiteral("sink")) {
            stage = QStringLiteral("h264parse-sink");
        } else if (elementName == QStringLiteral("h264-parse")) {
            stage = QStringLiteral("h264parse-src");
        } else if (elementName == QStringLiteral("h264-decoder") &&
                   padName == QStringLiteral("sink")) {
            stage = QStringLiteral("decoder-sink");
        } else if (elementName == QStringLiteral("h264-decoder") &&
                   padName == QStringLiteral("src")) {
            stage = QStringLiteral("decoder-src");
        }

        if (!stage.isEmpty()) {
            const qint64 nowNs = monotonicNs();
            const auto previous = self->m_stageLastNs.value(stage, -1);
            const qint64 intervalMs =
                previous >= 0 ? (nowNs - previous) / 1000000 : -1;
            self->m_stageLastNs.insert(stage, nowNs);

            const quint64 stageCount = ++self->m_stageCounts[stage];
            if (stageCount == 1 || (stageCount % 30) == 0) {
                Logger::instance().infoContext(
                    "H264VideoDecoder", "GStreamer stage cadence",
                    {{"stage", stage},
                     {"count", static_cast<qulonglong>(stageCount)},
                     {"interval_ms", intervalMs},
                     {"bytes", static_cast<qulonglong>(
                                   buffer ? gst_buffer_get_size(buffer) : 0)}});
            }
        }

        if (elementName == QStringLiteral("h264-source")) {
            count = &self->m_sourceBufferCount;
        } else if (elementName == QStringLiteral("h264-parse") && padName == QStringLiteral("sink")) {
            count = &self->m_parserInputBufferCount;
        } else if (elementName == QStringLiteral("h264-parse")) {
            count = &self->m_parserBufferCount;
        } else if (elementName == QStringLiteral("h264-decoder") && padName == QStringLiteral("sink")) {
            count = &self->m_decoderInputBufferCount;
            if (buffer) {
                const guint64 pts = GST_BUFFER_PTS_IS_VALID(buffer)
                    ? GST_BUFFER_PTS(buffer)
                    : GST_CLOCK_TIME_NONE;
                if (pts != GST_CLOCK_TIME_NONE) {
                    self->m_decoderStartTimesNs.insert(pts, monotonicNs());
                }
            }
        } else if (elementName == QStringLiteral("h264-decoder") && padName == QStringLiteral("src")) {
            count = &self->m_decoderOutputBufferCount;
            if (buffer) {
                const guint64 pts = GST_BUFFER_PTS_IS_VALID(buffer)
                    ? GST_BUFFER_PTS(buffer)
                    : GST_CLOCK_TIME_NONE;
                if (pts != GST_CLOCK_TIME_NONE) {
                    const auto it = self->m_decoderStartTimesNs.find(pts);
                    if (it != self->m_decoderStartTimesNs.end()) {
                        const qint64 elapsedUs = (monotonicNs() - it.value()) / 1000;
                        self->m_decoderStartTimesNs.erase(it);
                        ++self->m_decoderTimingSampleCount;
                        if (self->m_decoderTimingSampleCount == 1 ||
                            (self->m_decoderTimingSampleCount % 30) == 0) {
                            Logger::instance().infoContext(
                                "H264VideoDecoder", "H.264 decoder input-to-output timing",
                                {{"count", static_cast<qulonglong>(self->m_decoderTimingSampleCount)},
                                 {"elapsed_us", elapsedUs},
                                 {"elapsed_ms", elapsedUs / 1000.0},
                                 {"width", self->m_width},
                                 {"height", self->m_height}});
                        }
                    }
                }
            }
        }

        if (count) {
            ++(*count);
            if (*count == 1 || (*count % 120) == 0) {
                Logger::instance().infoContext(
                    "H264VideoDecoder", "GStreamer buffer observed",
                    {{"element", elementName},
                     {"pad", padName},
                     {"count", static_cast<qulonglong>(*count)},
                     {"bytes", static_cast<qulonglong>(buffer ? gst_buffer_get_size(buffer) : 0)}});
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

auto H264VideoDecoder::handleSample(GstSample* sample) -> GstFlowReturn {
    GstCaps* caps = gst_sample_get_caps(sample);
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!caps || !buffer) return GST_FLOW_ERROR;

    GstMapInfo map{};
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) return GST_FLOW_ERROR;

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        gst_buffer_unmap(buffer, &map);
        return GST_FLOW_ERROR;
    }

    const int width = static_cast<int>(GST_VIDEO_INFO_WIDTH(&info));
    const int height = static_cast<int>(GST_VIDEO_INFO_HEIGHT(&info));
    const int sourceStride = static_cast<int>(GST_VIDEO_INFO_PLANE_STRIDE(&info, 0));
    const int rowBytes = width * 4;

    ++m_sampleCount;
    if (m_sampleCount == 1 || (m_sampleCount % 120) == 0) {
        Logger::instance().infoContext(
            "H264VideoDecoder", "Received decoded RGBA sample from appsink",
            {{"count", static_cast<qulonglong>(m_sampleCount)},
             {"width", width},
             {"height", height},
             {"stride", sourceStride},
             {"bytes", static_cast<qulonglong>(map.size)}});
    }

    if (sourceStride < rowBytes || static_cast<gsize>(sourceStride * height) > map.size) {
        gst_buffer_unmap(buffer, &map);
        return GST_FLOW_ERROR;
    }

    const qint64 conversionStartNs = monotonicNs();
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        std::memcpy(image.scanLine(y), map.data + (y * sourceStride),
                    static_cast<size_t>(rowBytes));
    }
    const qint64 conversionElapsedUs = (monotonicNs() - conversionStartNs) / 1000;
    ++m_conversionTimingSampleCount;
    if (m_conversionTimingSampleCount == 1 ||
        (m_conversionTimingSampleCount % 30) == 0) {
        Logger::instance().infoContext(
            "H264VideoDecoder", "Decoded BGRA to QImage conversion timing",
            {{"count", static_cast<qulonglong>(m_conversionTimingSampleCount)},
             {"elapsed_us", conversionElapsedUs},
             {"elapsed_ms", conversionElapsedUs / 1000.0},
             {"width", width},
             {"height", height},
             {"source_stride", sourceStride},
             {"row_bytes", rowBytes}});
    }

    gst_buffer_unmap(buffer, &map);
    emit frameReady(image, width, height);
    return GST_FLOW_OK;
}

auto H264VideoDecoder::onBusMessage(GstBus*, GstMessage* message, gpointer userData) -> gboolean {
    auto* self = static_cast<H264VideoDecoder*>(userData);
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR ||
        GST_MESSAGE_TYPE(message) == GST_MESSAGE_WARNING) {
        GError* error = nullptr;
        gchar* debug = nullptr;
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            ++self->m_busErrorCount;
            gst_message_parse_error(message, &error, &debug);
        } else {
            ++self->m_busWarningCount;
            gst_message_parse_warning(message, &error, &debug);
        }
        self->handleError(error, debug);
        if (error) g_error_free(error);
        if (debug) g_free(debug);
    }
    return G_SOURCE_CONTINUE;
}

auto H264VideoDecoder::handleError(GError* error, const gchar* debug) -> void {
    const QString message = error ? QString::fromUtf8(error->message)
                                   : QStringLiteral("Unknown GStreamer error");
    Logger::instance().errorContext(
        "H264VideoDecoder", message,
        {{"debug", debug && *debug ? QString::fromUtf8(debug) : QString()},
         {"pushed_buffers", static_cast<qulonglong>(m_pushedFrameCount)},
         {"decoded_samples", static_cast<qulonglong>(m_sampleCount)},
         {"bus_errors", static_cast<qulonglong>(m_busErrorCount)},
         {"bus_warnings", static_cast<qulonglong>(m_busWarningCount)}});
    // cppcheck-suppress shadowFunction
    emit errorOccurred(debug && *debug
                           ? QStringLiteral("%1 (%2)").arg(message, QString::fromUtf8(debug))
                           : message);
}

auto H264VideoDecoder::stop() -> void {
    if (m_pipeline) gst_element_set_state(m_pipeline, GST_STATE_NULL);
}

auto H264VideoDecoder::teardownPipeline() -> void {
    if (m_pipeline) gst_element_set_state(m_pipeline, GST_STATE_NULL);
    if (m_bus) {
        gst_bus_remove_signal_watch(m_bus);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }
    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_appsrc = nullptr;
    m_h264parse = nullptr;
    m_decodebin = nullptr;
    m_videoconvert = nullptr;
    m_appsink = nullptr;
}

auto H264VideoDecoder::shutdown() -> void {
    teardownPipeline();
    // Deliberately leave m_hardwareDecoderUnavailable untouched: once we've
    // learned the hardware decoder can't be used on this device, keep using
    // software decode on every subsequent reconnect rather than re-probing
    // (and re-failing) it each time.
    m_initialised = false;
}
