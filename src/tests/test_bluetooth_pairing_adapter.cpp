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

#include <QtTest/QtTest>

#include "BluetoothAdapter.h"
#include "CoreClient.h"

namespace {

// Records what the adapter publishes to core, instead of sending it.
class RecordingCoreClient : public CoreClient {
 public:
  using CoreClient::CoreClient;

  struct Published {
    QString topic;
    QJsonObject payload;
  };
  QList<Published> published;

  auto publish(const QString& topic, const QJsonObject& payload) -> void override {
    published.append({topic, payload});
  }
};

}  // namespace

class BluetoothPairingAdapterTest : public QObject {
  Q_OBJECT

 private:
  static QVariantMap promptPayload(const QString& id, const QString& kind = QStringLiteral("pin")) {
    QVariantMap payload;
    payload[QStringLiteral("id")] = id;
    payload[QStringLiteral("kind")] = kind;
    payload[QStringLiteral("address")] = QStringLiteral("AA:BB:CC:DD:EE:FF");
    payload[QStringLiteral("name")] = QStringLiteral("Speaker");
    payload[QStringLiteral("timeout_seconds")] = 60;
    return payload;
  }

  static QVariantMap rejectedPayload(const QString& id, const QString& reason) {
    return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("reason"), reason}};
  }

 private slots:
  void promptRequestOpensPrompt() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    QSignalSpy changed(&adapter, &BluetoothAdapter::pairingPromptChanged);

    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(adapter.pairingPrompt().value(QStringLiteral("id")).toString(), QStringLiteral("1"));
    QCOMPARE(adapter.pairingPrompt().value(QStringLiteral("kind")).toString(),
             QStringLiteral("pin"));
  }

  void displayCodeIsShownUntilDismissed() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);

    emit client.bluetoothEventReceived(
        QStringLiteral("bluetooth/pairing/display"),
        QVariantMap{{QStringLiteral("address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                    {QStringLiteral("code"), QStringLiteral("012345")}});
    QCOMPARE(adapter.pairingDisplay().value(QStringLiteral("code")).toString(),
             QStringLiteral("012345"));

    adapter.dismissPairingDisplay();
    QVERIFY(adapter.pairingDisplay().isEmpty());
  }

  void closedEventForCurrentPromptClearsIt() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));

    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/closed"),
                                       QVariantMap{{QStringLiteral("id"), QStringLiteral("1")}});

    QVERIFY(adapter.pairingPrompt().isEmpty());
  }

  void closedEventForOtherPromptKeepsCurrentOne() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));

    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/closed"),
                                       QVariantMap{{QStringLiteral("id"), QStringLiteral("2")}});

    QCOMPARE(adapter.pairingPrompt().value(QStringLiteral("id")).toString(), QStringLiteral("1"));
  }

  void invalidAnswerKeepsPromptOpenForCorrection() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));

    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/rejected"),
                                       rejectedPayload(QStringLiteral("1"),
                                                       QStringLiteral("invalid_answer")));

    QVERIFY(!adapter.pairingPrompt().isEmpty());
    QVERIFY(!adapter.lastError().isEmpty());
  }

  void unknownPromptClearsItAndReportsExpiry() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));

    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/rejected"),
                                       rejectedPayload(QStringLiteral("1"),
                                                       QStringLiteral("unknown_prompt")));

    QVERIFY(adapter.pairingPrompt().isEmpty());
    QCOMPARE(adapter.lastError(), QStringLiteral("That pairing request has expired"));
  }

  void respondPublishesAnswerForCurrentPrompt() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("7")));
    client.published.clear();

    adapter.respondToPairing(true, QStringLiteral("0000"));

    QCOMPARE(client.published.size(), 1);
    QCOMPARE(client.published.first().topic, QStringLiteral("bluetooth/pairing/respond"));
    const QJsonObject payload = client.published.first().payload;
    QCOMPARE(payload.value(QStringLiteral("id")).toString(), QStringLiteral("7"));
    QCOMPARE(payload.value(QStringLiteral("accept")).toBool(), true);
    QCOMPARE(payload.value(QStringLiteral("answer")).toString(), QStringLiteral("0000"));
  }

  void respondWithoutPromptPublishesNothing() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    client.published.clear();

    adapter.respondToPairing(true, QStringLiteral("0000"));

    QVERIFY(client.published.isEmpty());
  }

  void pairResultClearsPendingPairingState() {
    RecordingCoreClient client;
    BluetoothAdapter adapter(&client);
    emit client.bluetoothEventReceived(QStringLiteral("bluetooth/pairing/request"),
                                       promptPayload(QStringLiteral("1")));
    emit client.bluetoothEventReceived(
        QStringLiteral("bluetooth/pairing/display"),
        QVariantMap{{QStringLiteral("address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                    {QStringLiteral("code"), QStringLiteral("012345")}});

    emit client.bluetoothEventReceived(
        QStringLiteral("bluetooth/pair/result"),
        QVariantMap{{QStringLiteral("success"), false},
                    {QStringLiteral("address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                    {QStringLiteral("error"), QStringLiteral("Failed to pair Bluetooth device")}});

    QVERIFY(adapter.pairingPrompt().isEmpty());
    QVERIFY(adapter.pairingDisplay().isEmpty());
  }
};

QTEST_MAIN(BluetoothPairingAdapterTest)
#include "test_bluetooth_pairing_adapter.moc"
