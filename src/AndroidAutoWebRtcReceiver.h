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

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <memory>

class AndroidAutoWebRtcSession;
#include "ProjectionVideoRenderer.h"

struct _GstPromise;
struct _GstElement;
struct _GstPad;
struct _GstSample;

class AndroidAutoWebRtcReceiver : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool healthy READ healthy NOTIFY healthyChanged)
    Q_PROPERTY(bool stalled READ stalled NOTIFY stalledChanged)
    Q_PROPERTY(bool recoverableError READ recoverableError NOTIFY recoverableErrorChanged)
    Q_PROPERTY(bool fallbackRecommended READ fallbackRecommended NOTIFY fallbackRecommendedChanged)
    Q_PROPERTY(qint64 lastFrameTimestampMs READ lastFrameTimestampMs NOTIFY lastFrameTimestampMsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QObject* videoSinkObject READ videoSinkObject CONSTANT)
    Q_PROPERTY(QString rendererBackend READ rendererBackend CONSTANT)

public:
    explicit AndroidAutoWebRtcReceiver(AndroidAutoWebRtcSession* session,
                                       std::unique_ptr<ProjectionVideoRenderer> renderer = {},
                                       QObject* parent = nullptr);
    ~AndroidAutoWebRtcReceiver() override;

    [[nodiscard]] auto active() const -> bool;
    [[nodiscard]] auto healthy() const -> bool;
    [[nodiscard]] auto stalled() const -> bool;
    [[nodiscard]] auto recoverableError() const -> bool;
    [[nodiscard]] auto fallbackRecommended() const -> bool;
    [[nodiscard]] auto lastFrameTimestampMs() const -> qint64;
    [[nodiscard]] auto lastError() const -> QString;
    [[nodiscard]] auto videoSinkObject() const -> QObject*;
    [[nodiscard]] auto rendererBackend() const -> QString;

signals:
    void activeChanged(bool active);
    void healthyChanged(bool healthy);
    void stalledChanged(bool stalled);
    void recoverableErrorChanged(bool recoverableError);
    void fallbackRecommendedChanged(bool fallbackRecommended);
    void lastFrameTimestampMsChanged(qint64 timestampMs);
    void lastErrorChanged(const QString& error);

private slots:
    void onSessionActiveChanged(bool active);
    void onRemoteOfferReceived(const QString& sdp, const QVariantMap& payload);
    void onRemoteIceCandidateReceived(const QVariantMap& payload);
    void onFrameStallTimeout();

private:
    void setActive(bool active);
    void setHealthy(bool healthy);
    void setStalled(bool stalled);
    void setRecoverableError(bool recoverableError);
    void setLastFrameTimestampMs(qint64 timestampMs);
    void refreshFallbackRecommendation();
    void markFrameReceived();
    void setLastError(const QString& error);
    auto ensurePipeline() -> bool;
    void teardownPipeline();

    static void onNegotiationAnswerCreated(struct _GstPromise* promise, void* userData);
    static void onIncomingPadAdded(struct _GstElement* element, struct _GstPad* pad,
                                   void* userData);
    static void onDecodebinPadAdded(struct _GstElement* element, struct _GstPad* pad,
                                    void* userData);
    static int onNewSample(void* sink, void* userData);
    static void onLocalIceCandidate(struct _GstElement* element, unsigned int mlineIndex,
                                    char* candidate, void* userData);

    void handleAnswerCreated(struct _GstPromise* promise);
    void handleIncomingPad(struct _GstPad* pad);
    void handleDecodebinPad(struct _GstPad* pad);
    auto pushSampleToRenderer(struct _GstSample* sample) -> int;

    AndroidAutoWebRtcSession* m_session{nullptr};
    std::unique_ptr<ProjectionVideoRenderer> m_renderer;
    bool m_active{false};
    bool m_healthy{false};
    bool m_stalled{false};
    bool m_recoverableError{false};
    bool m_fallbackRecommended{false};
    qint64 m_lastFrameTimestampMs{0};
    QString m_lastError;
    QTimer m_frameStallTimer;
    quint64 m_frameReceivedCount{0};
    qint64 m_firstFrameTimestampMs{0};
    qint64 m_lastFrameDeltaMs{0};

#if CRANKSHAFT_UI_GSTREAMER_WEBRTC
    struct _GstElement* m_pipeline{nullptr};
    struct _GstElement* m_webrtcBin{nullptr};
    struct _GstElement* m_incomingQueue{nullptr};
    struct _GstElement* m_h264Depay{nullptr};
    struct _GstElement* m_h264Parse{nullptr};
    struct _GstElement* m_decodeBin{nullptr};
    struct _GstElement* m_videoConvert{nullptr};
    struct _GstElement* m_appSink{nullptr};
    bool m_incomingPadLinked{false};
    bool m_decodePadLinked{false};
#endif
};