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

#include <QVariantMap>

class QString;

class Logger {
public:
	enum class Level {
		Debug = 0,
		Info = 1,
		Warning = 2,
		Error = 3
	};

	static auto instance() -> Logger&;

	auto setLevel(Level level) -> void;
	auto setLogFile(const QString& filePath) -> void;

	auto debugContext(const QString& component, const QString& message,
					  const QVariantMap& metadata = {}) -> void;
	auto infoContext(const QString& component, const QString& message,
					 const QVariantMap& metadata = {}) -> void;
	auto warningContext(const QString& component, const QString& message,
						const QVariantMap& metadata = {}) -> void;
	auto errorContext(const QString& component, const QString& message,
					  const QVariantMap& metadata = {}) -> void;

private:
	Logger() = default;

	auto log(Level level, const QString& component, const QString& message,
			 const QVariantMap& metadata) -> void;

	Level m_level = Level::Info;
	QString m_logFilePath;
};
