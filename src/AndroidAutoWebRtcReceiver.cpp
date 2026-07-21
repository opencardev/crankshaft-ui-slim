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

#include "AndroidAutoWebRtcReceiver.h"

#include <QDateTime>
#include <QImage>

#include "AndroidAutoWebRtcSession.h"
#include "Logger.h"
#include "QVideoSinkProjectionVideoRenderer.h"

namespace {
constexpr int kWebRtcFrameStallTimeoutMs = 900;
}

#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include <gst/video/video.h>
#include <gst/webrtc/webrtc.h>
#endif

AndroidAutoWebRtcReceiver::AndroidAutoWebRtcReceiver(
    AndroidAutoWebRtcSession* session, std::unique_ptr<ProjectionVideoRenderer> renderer,
    QObject* parent)
    : QObject(parent), m_session(session) {
        m_frameStallTimer.setSingleShot(true);
        m_frameStallTimer.setInterval(kWebRtcFrameStallTimeoutMs);
        connect(&m_frameStallTimer, &QTimer::timeout, this,
                &AndroidAutoWebRtcReceiver::onFrameStallTimeout);

    if (renderer) {
        m_renderer = std::move(renderer);
    } else {
        m_renderer = std::make_unique<QVideoSinkProjectionVideoRenderer>(this);
    }

#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    gst_init(nullptr, nullptr);
#endif

    if (!m_session) {
        setLastError(QStringLiteral("AndroidAutoWebRtcSession is required"));
        return;
    }

    connect(m_session, &AndroidAutoWebRtcSession::activeChanged, this,
            &AndroidAutoWebRtcReceiver::onSessionActiveChanged);
    connect(m_session, &AndroidAutoWebRtcSession::remoteOfferReceived, this,
            &AndroidAutoWebRtcReceiver::onRemoteOfferReceived);
    connect(m_session, &AndroidAutoWebRtcSession::remoteIceCandidateReceived, this,
            &AndroidAutoWebRtcReceiver::onRemoteIceCandidateReceived);

    onSessionActiveChanged(m_session->active());
}

AndroidAutoWebRtcReceiver::~AndroidAutoWebRtcReceiver() {
    teardownPipeline();
}

auto AndroidAutoWebRtcReceiver::active() const -> bool { return m_active; }

auto AndroidAutoWebRtcReceiver::healthy() const -> bool { return m_healthy; }

auto AndroidAutoWebRtcReceiver::stalled() const -> bool { return m_stalled; }

auto AndroidAutoWebRtcReceiver::recoverableError() const -> bool { return m_recoverableError; }

auto AndroidAutoWebRtcReceiver::fallbackRecommended() const -> bool {
    return m_fallbackRecommended;
}

auto AndroidAutoWebRtcReceiver::lastFrameTimestampMs() const -> qint64 {
    return m_lastFrameTimestampMs;
}

auto AndroidAutoWebRtcReceiver::lastError() const -> QString { return m_lastError; }

auto AndroidAutoWebRtcReceiver::videoSinkObject() const -> QObject* {
    return m_renderer ? m_renderer->surfaceObject() : nullptr;
}

auto AndroidAutoWebRtcReceiver::rendererBackend() const -> QString {
    return QStringLiteral("qvideosink");
}

void AndroidAutoWebRtcReceiver::onSessionActiveChanged(bool active) {
    setActive(active);

    if (!active) {
        m_frameStallTimer.stop();
        setHealthy(false);
        setStalled(false);
        setRecoverableError(false);
        setLastFrameTimestampMs(0);
        m_frameReceivedCount = 0;
        m_firstFrameTimestampMs = 0;
        m_lastFrameDeltaMs = 0;
        teardownPipeline();
        if (m_renderer) {
            m_renderer->clear();
        }
        return;
    }

    m_frameStallTimer.start();
}

void AndroidAutoWebRtcReceiver::onRemoteOfferReceived(const QString& sdp,
                                                      const QVariantMap& payload) {
    Q_UNUSED(payload)

    if (!m_active) {
        return;
    }

#if !CRANKSHAFT_UI_GSTREAMER_WEBRTC
    Q_UNUSED(sdp)
    setRecoverableError(true);
    setHealthy(false);
    setStalled(true);
    setLastError(QStringLiteral("ui-slim built without GStreamer WebRTC receiver support"));
    return;
#else
    if (!ensurePipeline()) {
        return;
    }

    const QByteArray sdpBytes = sdp.toUtf8();
    GstSDPMessage* sdpMessage = nullptr;
    if (gst_sdp_message_new(&sdpMessage) != GST_SDP_OK ||
        gst_sdp_message_parse_buffer(reinterpret_cast<const guint8*>(sdpBytes.constData()),
                                     sdpBytes.size(), sdpMessage) != GST_SDP_OK) {
        if (sdpMessage) {
            gst_sdp_message_free(sdpMessage);
        }
        setRecoverableError(true);
        setHealthy(false);
        setStalled(true);
        setLastError(QStringLiteral("Failed to parse remote WebRTC offer SDP"));
        return;
    }

    GstWebRTCSessionDescription* offer =
        gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdpMessage);
    GstPromise* setRemotePromise = gst_promise_new();
    g_signal_emit_by_name(m_webrtcBin, "set-remote-description", offer, setRemotePromise);
    gst_promise_interrupt(setRemotePromise);
    gst_promise_unref(setRemotePromise);
    gst_webrtc_session_description_free(offer);

    GstPromise* answerPromise =
        gst_promise_new_with_change_func(&AndroidAutoWebRtcReceiver::onNegotiationAnswerCreated,
                                         this, nullptr);
    g_signal_emit_by_name(m_webrtcBin, "create-answer", nullptr, answerPromise);
#endif
}

void AndroidAutoWebRtcReceiver::onRemoteIceCandidateReceived(const QVariantMap& payload) {
#if !CRANKSHAFT_UI_GSTREAMER_WEBRTC
    Q_UNUSED(payload)
    return;
#else
    if (!m_webrtcBin) {
        return;
    }

    const QString candidate = payload.value(QStringLiteral("candidate")).toString();
    const int mlineIndex = payload.value(QStringLiteral("sdpMLineIndex"), 0).toInt();
    if (candidate.trimmed().isEmpty() || mlineIndex < 0) {
        return;
    }

    g_signal_emit_by_name(m_webrtcBin, "add-ice-candidate", static_cast<guint>(mlineIndex),
                          candidate.toUtf8().constData());
#endif
}

void AndroidAutoWebRtcReceiver::setActive(bool active) {
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged(m_active);
    refreshFallbackRecommendation();
}

void AndroidAutoWebRtcReceiver::setHealthy(bool healthy) {
    if (m_healthy == healthy) {
        return;
    }

    m_healthy = healthy;
    emit healthyChanged(m_healthy);
    refreshFallbackRecommendation();
}

void AndroidAutoWebRtcReceiver::setStalled(bool stalled) {
    if (m_stalled == stalled) {
        return;
    }

    m_stalled = stalled;
    emit stalledChanged(m_stalled);
}

void AndroidAutoWebRtcReceiver::setRecoverableError(bool recoverableError) {
    if (m_recoverableError == recoverableError) {
        return;
    }

    m_recoverableError = recoverableError;
    emit recoverableErrorChanged(m_recoverableError);
    refreshFallbackRecommendation();
}

void AndroidAutoWebRtcReceiver::setLastFrameTimestampMs(qint64 timestampMs) {
    if (m_lastFrameTimestampMs == timestampMs) {
        return;
    }

    m_lastFrameTimestampMs = timestampMs;
    emit lastFrameTimestampMsChanged(m_lastFrameTimestampMs);
}

void AndroidAutoWebRtcReceiver::refreshFallbackRecommendation() {
    const bool fallback = m_active && (!m_healthy || m_recoverableError);
    if (m_fallbackRecommended == fallback) {
        return;
    }

    m_fallbackRecommended = fallback;
    emit fallbackRecommendedChanged(m_fallbackRecommended);
}

void AndroidAutoWebRtcReceiver::markFrameReceived() {
    if (!m_active) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_firstFrameTimestampMs == 0) {
        m_firstFrameTimestampMs = nowMs;
    }

    if (m_lastFrameTimestampMs > 0) {
        m_lastFrameDeltaMs = nowMs - m_lastFrameTimestampMs;
    }

    m_frameReceivedCount++;
    setLastFrameTimestampMs(nowMs);
    setStalled(false);
    setRecoverableError(false);
    setHealthy(true);
    m_frameStallTimer.start();

    if (m_frameReceivedCount == 1 || (m_frameReceivedCount % 120) == 0) {
        Logger::instance().debugContext(
            "AndroidAutoWebRtcReceiver",
            QString("frame_flow sample=%1 delta_ms=%2 first_frame_age_ms=%3")
                .arg(m_frameReceivedCount)
                .arg(m_lastFrameDeltaMs)
                .arg(nowMs - m_firstFrameTimestampMs));
    }
}

void AndroidAutoWebRtcReceiver::onFrameStallTimeout() {
    if (!m_active) {
        return;
    }

    if (!m_stalled) {
        Logger::instance().warningContext("AndroidAutoWebRtcReceiver",
                                          "WebRTC frame flow stalled; enabling fallback");
    }
    setStalled(true);
    setHealthy(false);
}

void AndroidAutoWebRtcReceiver::setLastError(const QString& error) {
    if (m_lastError == error) {
        return;
    }

    m_lastError = error;
    emit lastErrorChanged(m_lastError);
    if (!m_lastError.isEmpty()) {
        if (m_active) {
            setRecoverableError(true);
            setHealthy(false);
            setStalled(true);
        }
        Logger::instance().errorContext("AndroidAutoWebRtcReceiver", m_lastError);
    }
}

auto AndroidAutoWebRtcReceiver::ensurePipeline() -> bool {
#if !CRANKSHAFT_UI_GSTREAMER_WEBRTC
    return false;
#else
    if (m_pipeline) {
        return true;
    }

    m_pipeline = gst_pipeline_new("android-auto-webrtc-receiver");
    m_webrtcBin = gst_element_factory_make("webrtcbin", "webrtcbin");
    m_incomingQueue = gst_element_factory_make("queue", "incoming-queue");
    m_h264Depay = gst_element_factory_make("rtph264depay", "rtph264depay");
    m_h264Parse = gst_element_factory_make("h264parse", "h264parse");
    m_decodeBin = gst_element_factory_make("decodebin", "decodebin");
    m_videoConvert = gst_element_factory_make("videoconvert", "videoconvert");
    m_appSink = gst_element_factory_make("appsink", "appsink");

    if (!m_pipeline || !m_webrtcBin || !m_incomingQueue || !m_h264Depay || !m_h264Parse ||
        !m_decodeBin || !m_videoConvert || !m_appSink) {
        setLastError(QStringLiteral("Failed to create WebRTC receiver GStreamer elements"));
        teardownPipeline();
        return false;
    }

    g_object_set(G_OBJECT(m_webrtcBin), "latency", 50, nullptr);

    GstCaps* sinkCaps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA",
                                            nullptr);
    g_object_set(G_OBJECT(m_appSink), "emit-signals", TRUE, "sync", FALSE, "drop", TRUE,
                 "max-buffers", 1, "caps", sinkCaps, nullptr);
    gst_caps_unref(sinkCaps);

    gst_bin_add_many(GST_BIN(m_pipeline), m_webrtcBin, m_incomingQueue, m_h264Depay, m_h264Parse,
                     m_decodeBin, m_videoConvert, m_appSink, nullptr);

    if (!gst_element_link_many(m_incomingQueue, m_h264Depay, m_h264Parse, m_decodeBin, nullptr) ||
        !gst_element_link(m_videoConvert, m_appSink)) {
        setRecoverableError(true);
        setHealthy(false);
        setStalled(true);
        setLastError(QStringLiteral("Failed to link WebRTC receiver pipeline"));
        teardownPipeline();
        return false;
    }

    g_signal_connect(m_webrtcBin, "pad-added",
                     G_CALLBACK(&AndroidAutoWebRtcReceiver::onIncomingPadAdded), this);
    g_signal_connect(m_webrtcBin, "on-ice-candidate",
                     G_CALLBACK(&AndroidAutoWebRtcReceiver::onLocalIceCandidate), this);
    g_signal_connect(m_decodeBin, "pad-added",
                     G_CALLBACK(&AndroidAutoWebRtcReceiver::onDecodebinPadAdded), this);
    g_signal_connect(m_appSink, "new-sample", G_CALLBACK(&AndroidAutoWebRtcReceiver::onNewSample),
                     this);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        setRecoverableError(true);
        setHealthy(false);
        setStalled(true);
        setLastError(QStringLiteral("Failed to start WebRTC receiver pipeline"));
        teardownPipeline();
        return false;
    }

    return true;
#endif
}

void AndroidAutoWebRtcReceiver::teardownPipeline() {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    m_incomingPadLinked = false;
    m_decodePadLinked = false;

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }

    m_pipeline = nullptr;
    m_webrtcBin = nullptr;
    m_incomingQueue = nullptr;
    m_h264Depay = nullptr;
    m_h264Parse = nullptr;
    m_decodeBin = nullptr;
    m_videoConvert = nullptr;
    m_appSink = nullptr;
#endif
}

void AndroidAutoWebRtcReceiver::onNegotiationAnswerCreated(struct _GstPromise* promise,
                                                            void* userData) {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    auto* self = static_cast<AndroidAutoWebRtcReceiver*>(userData);
    self->handleAnswerCreated(promise);
#else
    Q_UNUSED(promise)
    Q_UNUSED(userData)
#endif
}

void AndroidAutoWebRtcReceiver::onIncomingPadAdded(struct _GstElement* element, struct _GstPad* pad,
                                                   void* userData) {
    Q_UNUSED(element)
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    auto* self = static_cast<AndroidAutoWebRtcReceiver*>(userData);
    self->handleIncomingPad(pad);
#else
    Q_UNUSED(pad)
    Q_UNUSED(userData)
#endif
}

void AndroidAutoWebRtcReceiver::onDecodebinPadAdded(struct _GstElement* element, struct _GstPad* pad,
                                                    void* userData) {
    Q_UNUSED(element)
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    auto* self = static_cast<AndroidAutoWebRtcReceiver*>(userData);
    self->handleDecodebinPad(pad);
#else
    Q_UNUSED(pad)
    Q_UNUSED(userData)
#endif
}

int AndroidAutoWebRtcReceiver::onNewSample(void* sink, void* userData) {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    auto* appSink = static_cast<GstAppSink*>(sink);
    auto* self = static_cast<AndroidAutoWebRtcReceiver*>(userData);
    GstSample* sample = gst_app_sink_pull_sample(appSink);
    if (!sample) {
        return GST_FLOW_ERROR;
    }
    const int result = self->pushSampleToRenderer(sample);
    gst_sample_unref(sample);
    return result;
#else
    Q_UNUSED(sink)
    Q_UNUSED(userData)
    return -1;
#endif
}

void AndroidAutoWebRtcReceiver::onLocalIceCandidate(struct _GstElement* element, unsigned int mlineIndex,
                                                    char* candidate, void* userData) {
    Q_UNUSED(element)
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    auto* self = static_cast<AndroidAutoWebRtcReceiver*>(userData);
    if (!self->m_session || !candidate) {
        return;
    }

    self->m_session->sendIceCandidate(QString::fromUtf8(candidate), static_cast<int>(mlineIndex),
                                      QStringLiteral("video"));
#else
    Q_UNUSED(mlineIndex)
    Q_UNUSED(candidate)
    Q_UNUSED(userData)
#endif
}

void AndroidAutoWebRtcReceiver::handleAnswerCreated(struct _GstPromise* promise) {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    const GstStructure* reply = gst_promise_get_reply(promise);
    if (!reply) {
        setLastError(QStringLiteral("WebRTC answer promise had no reply"));
        gst_promise_unref(promise);
        return;
    }

    GstWebRTCSessionDescription* answer = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, nullptr);
    if (!answer || !answer->sdp) {
        if (answer) {
            gst_webrtc_session_description_free(answer);
        }
        setLastError(QStringLiteral("Failed to create local WebRTC answer"));
        gst_promise_unref(promise);
        return;
    }

    GstPromise* setLocalPromise = gst_promise_new();
    g_signal_emit_by_name(m_webrtcBin, "set-local-description", answer, setLocalPromise);
    gst_promise_interrupt(setLocalPromise);
    gst_promise_unref(setLocalPromise);

    gchar* sdpText = gst_sdp_message_as_text(answer->sdp);
    if (m_session) {
        m_session->sendAnswer(QString::fromUtf8(sdpText));
    }
    g_free(sdpText);
    gst_webrtc_session_description_free(answer);
    gst_promise_unref(promise);
#else
    Q_UNUSED(promise)
#endif
}

void AndroidAutoWebRtcReceiver::handleIncomingPad(struct _GstPad* pad) {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    if (m_incomingPadLinked || !m_incomingQueue) {
        return;
    }

    GstPad* queueSinkPad = gst_element_get_static_pad(m_incomingQueue, "sink");
    if (!queueSinkPad) {
        return;
    }

    if (gst_pad_is_linked(queueSinkPad)) {
        gst_object_unref(queueSinkPad);
        return;
    }

    if (gst_pad_link(pad, queueSinkPad) == GST_PAD_LINK_OK) {
        m_incomingPadLinked = true;
    } else {
        setLastError(QStringLiteral("Failed to link incoming WebRTC RTP pad"));
    }

    gst_object_unref(queueSinkPad);
#else
    Q_UNUSED(pad)
#endif
}

void AndroidAutoWebRtcReceiver::handleDecodebinPad(struct _GstPad* pad) {
#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    if (m_decodePadLinked || !m_videoConvert) {
        return;
    }

    GstPad* convertSinkPad = gst_element_get_static_pad(m_videoConvert, "sink");
    if (!convertSinkPad) {
        return;
    }

    if (gst_pad_is_linked(convertSinkPad)) {
        gst_object_unref(convertSinkPad);
        return;
    }

    if (gst_pad_link(pad, convertSinkPad) == GST_PAD_LINK_OK) {
        m_decodePadLinked = true;
    } else {
        setLastError(QStringLiteral("Failed to link decoded video pad to renderer chain"));
    }

    gst_object_unref(convertSinkPad);
#else
    Q_UNUSED(pad)
#endif
}

auto AndroidAutoWebRtcReceiver::pushSampleToRenderer(struct _GstSample* sample) -> int {
#if !CRANKSHAFT_UI_GSTREAMER_WEBRTC
    Q_UNUSED(sample)
    return -1;
#else
    if (!sample || !m_renderer) {
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        return GST_FLOW_ERROR;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    int width = 0;
    int height = 0;
    if (!gst_structure_get_int(structure, "width", &width) ||
        !gst_structure_get_int(structure, "height", &height) || width <= 0 || height <= 0) {
        return GST_FLOW_ERROR;
    }

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        return GST_FLOW_ERROR;
    }

    QImage image(reinterpret_cast<const uchar*>(mapInfo.data), width, height,
                 QImage::Format_RGBA8888);
    QImage imageCopy = image.copy();
    gst_buffer_unmap(buffer, &mapInfo);

    if (imageCopy.isNull()) {
        return GST_FLOW_ERROR;
    }

    QMetaObject::invokeMethod(this,
                              [this, imageCopy]() {
                                  markFrameReceived();
                                  if (m_renderer) {
                                      m_renderer->presentImage(imageCopy);
                                  }
                              },
                              Qt::QueuedConnection);
    return GST_FLOW_OK;
#endif
}