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

#include "AndroidAutoWebRtcSession.h"

#include "AndroidAutoFacade.h"
#include "Logger.h"

namespace {
constexpr auto kStateIdle = "idle";
constexpr auto kStateHaveRemoteOffer = "have-remote-offer";
constexpr auto kStateLocalAnswerSent = "local-answer-sent";
constexpr auto kStateRemoteIceCandidateReceived = "remote-ice-candidate-received";
constexpr int kOfferWatchdogTimeoutMs = 1500;
}

AndroidAutoWebRtcSession::AndroidAutoWebRtcSession(AndroidAutoFacade* androidAutoFacade,
                                                   QObject* parent)
        : QObject(parent), m_androidAutoFacade(androidAutoFacade),
            m_signalingState(QString::fromLatin1(kStateIdle)) {
    m_offerWatchdogTimer.setSingleShot(true);
    m_offerWatchdogTimer.setInterval(kOfferWatchdogTimeoutMs);
    connect(&m_offerWatchdogTimer, &QTimer::timeout, this,
            &AndroidAutoWebRtcSession::onOfferWatchdogTimeout);

    if (!m_androidAutoFacade) {
        setLastError(QStringLiteral("AndroidAutoFacade is required for WebRTC session"));
        return;
    }

    connect(m_androidAutoFacade, &AndroidAutoFacade::videoTransportModeChanged, this,
            &AndroidAutoWebRtcSession::onVideoTransportModeChanged);
    connect(m_androidAutoFacade, &AndroidAutoFacade::webRtcSignalingReceived, this,
            &AndroidAutoWebRtcSession::onWebRtcSignalingReceived);
    connect(m_androidAutoFacade, &AndroidAutoFacade::connectionStateChanged, this,
            &AndroidAutoWebRtcSession::onConnectionStateChanged);

    onConnectionStateChanged(m_androidAutoFacade->connectionState());
    onVideoTransportModeChanged(m_androidAutoFacade->videoTransportMode());
}

auto AndroidAutoWebRtcSession::active() const -> bool { return m_active; }

auto AndroidAutoWebRtcSession::signalingState() const -> QString { return m_signalingState; }

auto AndroidAutoWebRtcSession::remoteOfferSdp() const -> QString { return m_remoteOfferSdp; }

auto AndroidAutoWebRtcSession::lastError() const -> QString { return m_lastError; }

void AndroidAutoWebRtcSession::sendAnswer(const QString& sdp) {
    if (!m_androidAutoFacade) {
        setLastError(QStringLiteral("AndroidAutoFacade unavailable"));
        return;
    }

    if (sdp.trimmed().isEmpty()) {
        setLastError(QStringLiteral("WebRTC answer SDP cannot be empty"));
        return;
    }

    m_androidAutoFacade->sendWebRtcSignalingMessage(
        QStringLiteral("android-auto/webrtc/answer"),
        QVariantMap{{QStringLiteral("type"), QStringLiteral("answer")},
                    {QStringLiteral("sdp"), sdp}});
    setSignalingState(QString::fromLatin1(kStateLocalAnswerSent));
}

void AndroidAutoWebRtcSession::sendIceCandidate(const QString& candidate, int sdpMLineIndex,
                                                const QString& sdpMid) {
    if (!m_androidAutoFacade) {
        setLastError(QStringLiteral("AndroidAutoFacade unavailable"));
        return;
    }

    if (candidate.trimmed().isEmpty()) {
        setLastError(QStringLiteral("WebRTC ICE candidate cannot be empty"));
        return;
    }

    m_androidAutoFacade->sendWebRtcSignalingMessage(
        QStringLiteral("android-auto/webrtc/ice-candidate"),
        QVariantMap{{QStringLiteral("type"), QStringLiteral("ice-candidate")},
                    {QStringLiteral("candidate"), candidate},
                    {QStringLiteral("sdpMLineIndex"), sdpMLineIndex},
                    {QStringLiteral("sdpMid"), sdpMid}});
}

void AndroidAutoWebRtcSession::resetSession() {
    const bool hadOffer = !m_remoteOfferSdp.isEmpty();
    disarmOfferWatchdog();
    m_renegotiationRequested = false;
    m_remoteOfferSdp.clear();
    if (hadOffer) {
        emit remoteOfferSdpChanged(m_remoteOfferSdp);
    }
    setSignalingState(QString::fromLatin1(kStateIdle));
    setLastError(QString());
}

void AndroidAutoWebRtcSession::onVideoTransportModeChanged(const QString& mode) {
    m_transportModeWebRtc =
        mode.compare(QStringLiteral("webrtc"), Qt::CaseInsensitive) == 0;
    refreshActiveState();
}

void AndroidAutoWebRtcSession::onWebRtcSignalingReceived(const QString& topic,
                                                         const QVariantMap& payload) {
    if (topic == QStringLiteral("android-auto/webrtc/offer")) {
        const QString sdp = payload.value(QStringLiteral("sdp")).toString();
        if (sdp.trimmed().isEmpty()) {
            setLastError(QStringLiteral("Received WebRTC offer without SDP"));
            return;
        }

        disarmOfferWatchdog();
        m_renegotiationRequested = false;
        Logger::instance().infoContext(
            "AndroidAutoWebRtcSession",
            QString("Received remote WebRTC offer (sdp_length=%1)").arg(sdp.size()));

        if (m_remoteOfferSdp != sdp) {
            m_remoteOfferSdp = sdp;
            emit remoteOfferSdpChanged(m_remoteOfferSdp);
        }
        setSignalingState(QString::fromLatin1(kStateHaveRemoteOffer));
        emit remoteOfferReceived(sdp, payload);
        return;
    }

    if (topic == QStringLiteral("android-auto/webrtc/ice-candidate")) {
        setSignalingState(QString::fromLatin1(kStateRemoteIceCandidateReceived));
        emit remoteIceCandidateReceived(payload);
    }
}

void AndroidAutoWebRtcSession::onConnectionStateChanged(int state) {
    m_connectionReady = (state == AndroidAutoFacade::Connected);
    refreshActiveState();

    if (state == AndroidAutoFacade::Disconnected || state == AndroidAutoFacade::Error) {
        resetSession();
    }
}

void AndroidAutoWebRtcSession::refreshActiveState() {
    setActive(m_transportModeWebRtc && m_connectionReady);
}

void AndroidAutoWebRtcSession::setActive(bool active) {
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged(m_active);

    if (m_active) {
        Logger::instance().infoContext(
            "AndroidAutoWebRtcSession",
            QString("WebRTC session became active (state=%1, hasOffer=%2)")
                .arg(m_signalingState)
                .arg(m_remoteOfferSdp.isEmpty() ? QStringLiteral("false") : QStringLiteral("true")));
        if (m_remoteOfferSdp.isEmpty()) {
            armOfferWatchdog();
        }
    } else {
        disarmOfferWatchdog();
    }
}

void AndroidAutoWebRtcSession::setSignalingState(const QString& state) {
    if (m_signalingState == state) {
        return;
    }

    m_signalingState = state;
    emit signalingStateChanged(m_signalingState);
}

void AndroidAutoWebRtcSession::setLastError(const QString& error) {
    if (m_lastError == error) {
        return;
    }

    m_lastError = error;
    emit lastErrorChanged(m_lastError);
}

void AndroidAutoWebRtcSession::armOfferWatchdog() {
    if (!m_active || !m_remoteOfferSdp.isEmpty()) {
        return;
    }

    if (!m_offerWatchdogTimer.isActive()) {
        Logger::instance().warningContext(
            "AndroidAutoWebRtcSession",
            QString("Arming WebRTC offer watchdog (%1 ms) while waiting for remote offer")
                .arg(kOfferWatchdogTimeoutMs));
        m_offerWatchdogTimer.start();
    }
}

void AndroidAutoWebRtcSession::disarmOfferWatchdog() {
    if (m_offerWatchdogTimer.isActive()) {
        m_offerWatchdogTimer.stop();
    }
}

void AndroidAutoWebRtcSession::onOfferWatchdogTimeout() {
    if (!m_active || !m_remoteOfferSdp.isEmpty() || m_renegotiationRequested || !m_androidAutoFacade) {
        return;
    }

    m_renegotiationRequested = true;
    Logger::instance().warningContext(
        "AndroidAutoWebRtcSession",
        "No remote WebRTC offer received after activation; requesting renegotiation");
    m_androidAutoFacade->requestRenegotiation();
}