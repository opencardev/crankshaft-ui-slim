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
#include <QVariantMap>

class AndroidAutoFacade;

class AndroidAutoWebRtcSession : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString signalingState READ signalingState NOTIFY signalingStateChanged)
    Q_PROPERTY(QString remoteOfferSdp READ remoteOfferSdp NOTIFY remoteOfferSdpChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AndroidAutoWebRtcSession(AndroidAutoFacade* androidAutoFacade,
                                      QObject* parent = nullptr);
    ~AndroidAutoWebRtcSession() override = default;

    [[nodiscard]] auto active() const -> bool;
    [[nodiscard]] auto signalingState() const -> QString;
    [[nodiscard]] auto remoteOfferSdp() const -> QString;
    [[nodiscard]] auto lastError() const -> QString;

    Q_INVOKABLE void sendAnswer(const QString& sdp);
    Q_INVOKABLE void sendIceCandidate(const QString& candidate, int sdpMLineIndex,
                                      const QString& sdpMid = QStringLiteral("video"));
    Q_INVOKABLE void resetSession();

signals:
    void activeChanged(bool active);
    void signalingStateChanged(const QString& state);
    void remoteOfferSdpChanged(const QString& sdp);
    void lastErrorChanged(const QString& error);
    void remoteOfferReceived(const QString& sdp, const QVariantMap& payload);
    void remoteIceCandidateReceived(const QVariantMap& payload);

private slots:
    void onVideoTransportModeChanged(const QString& mode);
    void onWebRtcSignalingReceived(const QString& topic, const QVariantMap& payload);
    void onConnectionStateChanged(int state);

private:
    void setActive(bool active);
    void setSignalingState(const QString& state);
    void setLastError(const QString& error);
    void refreshActiveState();

    AndroidAutoFacade* m_androidAutoFacade;
    bool m_active{false};
    bool m_transportModeWebRtc{false};
    bool m_connectionReady{false};
    QString m_signalingState;
    QString m_remoteOfferSdp;
    QString m_lastError;
};