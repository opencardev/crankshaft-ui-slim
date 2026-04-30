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

#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

namespace {
auto toLevelString(Logger::Level level) -> QString {
    switch (level) {
        case Logger::Level::Debug:
            return QStringLiteral("DEBUG");
        case Logger::Level::Info:
            return QStringLiteral("INFO");
        case Logger::Level::Warning:
            return QStringLiteral("WARNING");
        case Logger::Level::Error:
            return QStringLiteral("ERROR");
    }

    return QStringLiteral("INFO");
}
}  // namespace

auto Logger::instance() -> Logger& {
    static Logger logger;
    return logger;
}

auto Logger::setLevel(Level level) -> void { m_level = level; }

auto Logger::setLogFile(const QString& filePath) -> void {
    m_logFilePath = filePath;

    if (m_logFilePath.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(m_logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

auto Logger::debugContext(const QString& component, const QString& message,
                          const QVariantMap& metadata) -> void {
    log(Level::Debug, component, message, metadata);
}

auto Logger::infoContext(const QString& component, const QString& message,
                         const QVariantMap& metadata) -> void {
    log(Level::Info, component, message, metadata);
}

auto Logger::warningContext(const QString& component, const QString& message,
                            const QVariantMap& metadata) -> void {
    log(Level::Warning, component, message, metadata);
}

auto Logger::errorContext(const QString& component, const QString& message,
                          const QVariantMap& metadata) -> void {
    log(Level::Error, component, message, metadata);
}

auto Logger::log(Level level, const QString& component, const QString& message,
                 const QVariantMap& metadata) -> void {
    if (static_cast<int>(level) < static_cast<int>(m_level)) {
        return;
    }

    QJsonObject entry{
        {QStringLiteral("component"), component},
        {QStringLiteral("level"), toLevelString(level)},
        {QStringLiteral("message"), message},
        {QStringLiteral("timestamp"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("thread"), QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()))},
    };

    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        entry.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    const QString line = QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact));
    QTextStream(stdout) << line << Qt::endl;

    if (!m_logFilePath.isEmpty()) {
        QFile file(m_logFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << line << Qt::endl;
        }
    }
}