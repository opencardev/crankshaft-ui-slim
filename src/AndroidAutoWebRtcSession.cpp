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

namespace {
constexpr auto kStateIdle = "idle";
constexpr auto kStateHaveRemoteOffer = "have-remote-offer";
constexpr auto kStateLocalAnswerSent = "local-answer-sent";
constexpr auto kStateRemoteIceCandidateReceived = "remote-ice-candidate-received";
}

AndroidAutoWebRtcSession::AndroidAutoWebRtcSession(AndroidAutoFacade* androidAutoFacade,
                                                   QObject* parent)
        : QObject(parent), m_androidAutoFacade(androidAutoFacade),
            m_signalingState(QString::fromLatin1(kStateIdle)) {
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
    m_remoteOfferSdp.clear();
    if (hadOffer) {
        emit remoteOfferSdpChanged(m_remoteOfferSdp);
    }
    setSignalingState(QString::fromLatin1(kStateIdle));
    setLastError(QString());
}

void AndroidAutoWebRtcSession::onVideoTransportModeChanged(const QString& mode) {
    setActive(mode.compare(QStringLiteral("webrtc"), Qt::CaseInsensitive) == 0);
}

void AndroidAutoWebRtcSession::onWebRtcSignalingReceived(const QString& topic,
                                                         const QVariantMap& payload) {
    if (topic == QStringLiteral("android-auto/webrtc/offer")) {
        const QString sdp = payload.value(QStringLiteral("sdp")).toString();
        if (sdp.trimmed().isEmpty()) {
            setLastError(QStringLiteral("Received WebRTC offer without SDP"));
            return;
        }

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
    if (state == AndroidAutoFacade::Disconnected || state == AndroidAutoFacade::Error) {
        resetSession();
    }
}

void AndroidAutoWebRtcSession::setActive(bool active) {
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged(m_active);
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