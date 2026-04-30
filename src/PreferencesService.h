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

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

class QSettings;

class PreferencesService : public QObject {
  Q_OBJECT

 public:
  explicit PreferencesService(const QString& dbPath = QString(), QObject* parent = nullptr);
  ~PreferencesService() override;

  [[nodiscard]] auto initialize() -> bool;
  [[nodiscard]] auto get(const QString& key, const QVariant& defaultValue = QVariant()) const
      -> QVariant;
  [[nodiscard]] auto set(const QString& key, const QVariant& value) -> bool;
  [[nodiscard]] auto contains(const QString& key) const -> bool;
  [[nodiscard]] auto remove(const QString& key) -> bool;
  [[nodiscard]] auto clear() -> bool;
  [[nodiscard]] auto allKeys() const -> QStringList;

 signals:
  void preferenceChanged(const QString& key, const QVariant& newValue);

 private:
  QString m_dbPath;
  QSettings* m_settings = nullptr;
  QMap<QString, QVariant> m_cache;
};
