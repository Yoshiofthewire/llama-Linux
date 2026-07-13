#include "mail/MailController.h"

#include "db/Database.h"
#include "db/EmailDao.h"
#include "domain/DevicePairing.h"
#include "domain/KeywordRepository.h"
#include "domain/MailRepository.h"
#include "domain/PairingStore.h"
#include "mail/EmailListModel.h"
#include "net/HttpClient.h"
#include "net/RelayMailSource.h"
#include "stores/CursorStore.h"
#include "stores/SecureStoreFile.h"
#include "stores/SettingsStore.h"

#include "../../core/net/FakeRelayServer.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class MailControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void selectKeywordFiltersCachedEmailsWithoutAnyNetworkCall();
    void archiveEmailsNotPairedShortCircuitsWithNoNetworkCall();
    void sendMailOverAttachmentCapRejectsBeforeAnyNetworkCall();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void MailControllerTest::savePairing(PairingStore& pairingStore, quint16 port)
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-1");
    pairing.subscriberHash = QStringLiteral("hash-1");
    pairing.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    pairing.deviceId = QStringLiteral("dev-1");
    QVERIFY(pairingStore.save(pairing));
}

void MailControllerTest::selectKeywordFiltersCachedEmailsWithoutAnyNetworkCall()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    EmailDao emailDao(db.handle());

    // "Work" tab has m1, the display-only "Uncategorized" fallback tab has
    // m2 -- so after refresh() m1 carries keywords=["Work"], m2 carries no
    // keywords at all (matches MailRepository's existing keyword-population
    // rule, see MailRepositoryTest.cpp).
    const QByteArray body = R"(
    {
      "tabs": ["Work", "Uncategorized"],
      "byTab": {
        "Work": [
          {
            "messageId": "m1",
            "sender": "a@example.com",
            "sentTo": "b@example.com",
            "cc": "",
            "bcc": "",
            "subject": "Work item",
            "status": "unread",
            "atUtc": "2026-07-01T00:00:00Z",
            "hasAttachments": false,
            "label": ""
          }
        ],
        "Uncategorized": [
          {
            "messageId": "m2",
            "sender": "c@example.com",
            "sentTo": "d@example.com",
            "cc": "",
            "bcc": "",
            "subject": "Solo item",
            "status": "unread",
            "atUtc": "2026-07-02T00:00:00Z",
            "hasAttachments": false,
            "label": ""
          }
        ]
      }
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursor.ini")));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    KeywordRepository keywordRepository(settingsStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);
    MailRepository mailRepository(source, emailDao, pairingStore, cursorStore);

    MailController controller(mailRepository, source, keywordRepository, pairingStore);

    // refresh() is the only call in this test allowed to reach the network
    // -- it populates the cache selectKeyword() below must filter locally.
    controller.refresh();
    auto* model = qobject_cast<EmailListModel*>(controller.emailModel());
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 2);

    const QByteArray requestAfterRefresh = fake.receivedRequest();
    QVERIFY(!requestAfterRefresh.isEmpty());

    controller.selectKeyword(QStringLiteral("Work"));

    QCOMPARE(controller.selectedKeyword(), QStringLiteral("Work"));
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->emailAt(0).messageId, QStringLiteral("m1"));

    // No second connection was made -- FakeRelayServer's captured buffer is
    // unchanged (it can only append via a new connection's readyRead).
    QCOMPARE(fake.receivedRequest(), requestAfterRefresh);

    // Clearing the keyword restores both cached rows, still with no network.
    controller.selectKeyword(QString());
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(fake.receivedRequest(), requestAfterRefresh);
}

void MailControllerTest::archiveEmailsNotPairedShortCircuitsWithNoNetworkCall()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    EmailDao emailDao(db.handle());

    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"action":"archive","processed":0,"failed":[],"targetMailbox":""})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursor.ini")));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    KeywordRepository keywordRepository(settingsStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);
    MailRepository mailRepository(source, emailDao, pairingStore, cursorStore);

    MailController controller(mailRepository, source, keywordRepository, pairingStore);

    QSignalSpy errorSpy(&controller, &MailController::lastErrorChanged);
    const bool ok = controller.archiveEmails({ QStringLiteral("m1") });

    QCOMPARE(ok, false);
    QCOMPARE(controller.lastError(), QStringLiteral("Not paired"));
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(fake.receivedRequest().isEmpty());
}

void MailControllerTest::sendMailOverAttachmentCapRejectsBeforeAnyNetworkCall()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    EmailDao emailDao(db.handle());

    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"sentSaved":true,"warning":""})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursor.ini")));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    KeywordRepository keywordRepository(settingsStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);
    MailRepository mailRepository(source, emailDao, pairingStore, cursorStore);

    MailController controller(mailRepository, source, keywordRepository, pairingStore);

    QTemporaryDir attachmentDir;
    QVERIFY(attachmentDir.isValid());
    const QString bigFilePath = attachmentDir.filePath(QStringLiteral("big.bin"));
    QFile bigFile(bigFilePath);
    QVERIFY(bigFile.open(QIODevice::WriteOnly));
    // One byte over the 25 MB cap -- sparse-zero-filled, so this stays fast
    // and small on disk regardless of the logical size reported back.
    QVERIFY(bigFile.resize(25LL * 1024 * 1024 + 1));
    bigFile.close();

    const bool ok = controller.sendMail(QStringLiteral("to@example.com"), QString(), QString(),
                                         QStringLiteral("Subject"), QStringLiteral("Body"), { bigFilePath });

    QCOMPARE(ok, false);
    QVERIFY(controller.lastError().contains(QStringLiteral("25 MB")));
    QVERIFY(fake.receivedRequest().isEmpty());
}

QTEST_GUILESS_MAIN(MailControllerTest)
#include "MailControllerTest.moc"
