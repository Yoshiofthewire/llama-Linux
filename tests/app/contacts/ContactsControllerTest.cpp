#include "contacts/ContactsController.h"

#include "contacts/ContactListModel.h"
#include "db/ContactDao.h"
#include "db/Database.h"
#include "db/GroupDao.h"
#include "db/PendingContactChangeDao.h"
#include "domain/ContactSyncRepository.h"
#include "domain/DevicePairing.h"
#include "domain/GroupsRepository.h"
#include "domain/PairingStore.h"
#include "models/Group.h"
#include "net/ContactSyncClient.h"
#include "net/GroupsClient.h"
#include "net/HttpClient.h"
#include "stores/CursorStore.h"
#include "stores/SecureStoreFile.h"

#include "../../core/net/FakeRelayServer.h"

#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>
#include <utility>

namespace {

// Review follow-up (Task 2): the shared single-response FakeRelayServer
// (tests/core/net/FakeRelayServer.h) can't be reused as-is for
// syncSuccessRefreshesGroupsCache below, because that test drives
// ContactsController::sync() end to end -- which fires *two* sequential
// outbound requests against the same paired serverBaseUrl (first
// ContactSyncRepository::sync()'s GET /api/contacts/sync pull, then, only on
// Success, GroupsRepository::refresh()'s GET /api/groups) and each needs its
// own canned response shape. This is a small purpose-built variant, local to
// this test file, that dispatches on the request path instead of always
// replaying one fixed response. Both requests here are plain GETs (no
// pending changes queued -> ContactSyncRepository::sync() takes the pull(),
// not push(), branch), so -- unlike FakeRelayServer -- this doesn't need to
// wait for a Content-Length body; the header terminator is enough to know
// the request is complete.
class DualPathFakeRelayServer : public QObject
{
public:
    DualPathFakeRelayServer(QByteArray contactsSyncResponse, QByteArray groupsResponse)
        : m_contactsSyncResponse(std::move(contactsSyncResponse))
        , m_groupsResponse(std::move(groupsResponse))
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, &DualPathFakeRelayServer::onNewConnection);
    }

    quint16 port() const { return m_server.serverPort(); }
    bool groupsRequestReceived() const { return m_groupsRequestReceived; }
    bool contactsSyncRequestReceived() const { return m_contactsSyncRequestReceived; }

private:
    void onNewConnection()
    {
        QTcpSocket* socket = m_server.nextPendingConnection();
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
            *buffer += socket->readAll();
            if (buffer->indexOf("\r\n\r\n") < 0)
                return; // wait for the full header block
            const bool isGroupsRequest = buffer->contains("/api/groups");
            if (isGroupsRequest)
                m_groupsRequestReceived = true;
            else if (buffer->contains("/api/contacts/sync"))
                m_contactsSyncRequestReceived = true;
            socket->write(isGroupsRequest ? m_groupsResponse : m_contactsSyncResponse);
            socket->flush();
            socket->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    QByteArray m_contactsSyncResponse;
    QByteArray m_groupsResponse;
    bool m_groupsRequestReceived = false;
    bool m_contactsSyncRequestReceived = false;
};

} // namespace

class ContactsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void updateContactPreservesEmailEntriesBeyondIndexZero();
    void createContactRejectsBlankName();
    void updateContactRejectsBlankName();
    void syncWithoutPairingSetsNotPairedMessage();
    void syncSuccessRefreshesGroupsCache();
    void createAndUpdateContactRoundTripExtendedFields();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void ContactsControllerTest::savePairing(PairingStore& pairingStore, quint16 port)
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-1");
    pairing.subscriberHash = QStringLiteral("hash-1");
    pairing.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    pairing.registrationUrl = QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(port);
    pairing.pairingToken = QStringLiteral("pair-tok");
    pairing.deviceId = QStringLiteral("device-1");
    pairing.deviceName = QStringLiteral("My Linux Desktop");
    QVERIFY(pairingStore.save(pairing));
}

void ContactsControllerTest::updateContactPreservesEmailEntriesBeyondIndexZero()
{
    // Regression test for the "preserve extras beyond index 0" rule --
    // this is the one rule most likely to regress silently, per the task
    // brief.
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired, fine: this test never syncs

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);

    Contact seed;
    seed.uid = QStringLiteral("srv-1");
    seed.rev = 1; // already synced
    seed.fn = QStringLiteral("Old Name");
    seed.emails = { ContactEmailEntry{ std::nullopt, QStringLiteral("old@example.com") },
                     ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("extra@example.com") } };
    QVERIFY(contactDao.insertOrReplace(seed));

    ContactsController controller(repository, groupsRepository);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("New Name");
    fields[QStringLiteral("org")] = QString();
    fields[QStringLiteral("notes")] = QString();
    fields[QStringLiteral("email")] = QStringLiteral("new@example.com");
    fields[QStringLiteral("phone")] = QString();

    QVERIFY(controller.updateContact(QStringLiteral("srv-1"), fields));

    const QVariantMap updated = controller.contactAt(QStringLiteral("srv-1"));
    QCOMPARE(updated.value(QStringLiteral("fn")).toString(), QStringLiteral("New Name"));

    const QVariantList emails = updated.value(QStringLiteral("emails")).toList();
    QCOMPARE(emails.size(), 2);
    QCOMPARE(emails.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("new@example.com"));
    // index 1 survives unchanged, including its label.
    QCOMPARE(emails.at(1).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("extra@example.com"));
    QCOMPARE(emails.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("work"));

    // The model was reloaded, too.
    auto* model = qobject_cast<ContactListModel*>(controller.contactModel());
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0, 0), ContactListModel::FnRole).toString(), QStringLiteral("New Name"));
}

void ContactsControllerTest::createContactRejectsBlankName()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository, groupsRepository);

    QSignalSpy errorSpy(&controller, &ContactsController::lastErrorChanged);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("   "); // whitespace-only

    const QString newUid = controller.createContact(fields);

    QVERIFY(newUid.isEmpty());
    QCOMPARE(controller.lastError(), QStringLiteral("Name is required"));
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(repository.contacts().isEmpty());
}

void ContactsControllerTest::updateContactRejectsBlankName()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);

    Contact seed;
    seed.uid = QStringLiteral("srv-1");
    seed.rev = 1;
    seed.fn = QStringLiteral("Old Name");
    QVERIFY(contactDao.insertOrReplace(seed));

    ContactsController controller(repository, groupsRepository);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QString();

    QCOMPARE(controller.updateContact(QStringLiteral("srv-1"), fields), false);
    QCOMPARE(controller.lastError(), QStringLiteral("Name is required"));

    // The seed row is untouched.
    const QVariantMap unchanged = controller.contactAt(QStringLiteral("srv-1"));
    QCOMPARE(unchanged.value(QStringLiteral("fn")).toString(), QStringLiteral("Old Name"));
}

void ContactsControllerTest::syncWithoutPairingSetsNotPairedMessage()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository, groupsRepository);

    QSignalSpy errorSpy(&controller, &ContactsController::lastErrorChanged);
    QSignalSpy statusSpy(&controller, &ContactsController::statusMessageChanged);
    QSignalSpy busySpy(&controller, &ContactsController::isBusyChanged);

    controller.sync();

    QCOMPARE(controller.lastError(), QStringLiteral("Not paired"));
    QCOMPARE(controller.statusMessage(), QString());
    QVERIFY(errorSpy.count() >= 1);
    QCOMPARE(statusSpy.count(), 0); // was already "", stayed ""
    // isBusy toggled true then back to false around the (short-circuited,
    // no-network) sync() call.
    QVERIFY(busySpy.count() >= 2);
    QCOMPARE(controller.isBusy(), false);
}

void ContactsControllerTest::syncSuccessRefreshesGroupsCache()
{
    // Review follow-up (Task 2 finding): the line this whole GroupsRepository
    // design decision exists to wire up -- ContactsController::sync() calling
    // m_groupsRepository.refresh() in the ContactSyncStatus::Success branch
    // -- previously had zero automated regression protection, since every
    // existing sync() test here hit the NotPaired branch. This drives sync()
    // through a real Success outcome (valid pairing, no pending changes so
    // ContactSyncRepository::sync() takes the GET pull() branch) and asserts
    // the *observable effect* of refresh() having actually run: the groups
    // fetched from the fake server land in GroupsRepository::groups()/
    // GroupDao's cache. A future refactor that drops or misplaces the
    // refresh() call would leave the cache empty and fail this test.
    const QByteArray contactsSyncResponse =
        httpResponse(200, "OK", R"({"cursor":1,"tooOld":false,"changed":[],"deleted":[]})");
    const QByteArray groupsResponse =
        httpResponse(200, "OK", R"([{"id":"group-1","name":"Family","rev":1}])");
    DualPathFakeRelayServer fake(contactsSyncResponse, groupsResponse);

    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository, groupsRepository);

    QVERIFY(groupsRepository.groups().isEmpty()); // nothing cached before sync()

    controller.sync();

    QCOMPARE(controller.lastError(), QString());
    QCOMPARE(controller.statusMessage(), QStringLiteral("Synced -- 0 pushed, 0 applied"));

    QVERIFY(fake.contactsSyncRequestReceived());
    QVERIFY(fake.groupsRequestReceived()); // proves refresh() was actually invoked

    const QVector<Group> cachedGroups = groupsRepository.groups();
    QCOMPARE(cachedGroups.size(), 1);
    QCOMPARE(cachedGroups.at(0).id, QStringLiteral("group-1"));
    QCOMPARE(cachedGroups.at(0).name, QStringLiteral("Family"));
}

void ContactsControllerTest::createAndUpdateContactRoundTripExtendedFields()
{
    // Task 1 of the extended-contact-fields feature: no edit-form UI
    // consumes these 12 new QVariantMap keys yet, but createContact/
    // updateContact/contactAt must round-trip them correctly since a later
    // task's edit form will rely on this contract.
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    GroupDao groupDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);
    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository, groupsRepository);

    QVariantMap imEntry;
    imEntry[QStringLiteral("service")] = QStringLiteral("Matrix");
    imEntry[QStringLiteral("label")] = QStringLiteral("work");
    imEntry[QStringLiteral("value")] = QStringLiteral("@ada:example.org");

    QVariantMap websiteEntry;
    websiteEntry[QStringLiteral("label")] = QStringLiteral("blog");
    websiteEntry[QStringLiteral("value")] = QStringLiteral("https://ada.example.com");

    QVariantMap relationEntry;
    relationEntry[QStringLiteral("label")] = QStringLiteral("spouse");
    relationEntry[QStringLiteral("name")] = QStringLiteral("William King");

    QVariantMap eventEntry;
    eventEntry[QStringLiteral("label")] = QStringLiteral("anniversary");
    eventEntry[QStringLiteral("date")] = QStringLiteral("2026-06-01");

    QVariantMap customFieldEntry;
    customFieldEntry[QStringLiteral("label")] = QStringLiteral("Employee ID");
    customFieldEntry[QStringLiteral("value")] = QStringLiteral("42");

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("Ada Lovelace");
    fields[QStringLiteral("groupIds")] = QVariantList{ QStringLiteral("group-1"), QStringLiteral("group-2") };
    fields[QStringLiteral("photoRef")] = QStringLiteral("photo-ref-1");
    fields[QStringLiteral("pgpKey")] = QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----");
    fields[QStringLiteral("ims")] = QVariantList{ imEntry };
    fields[QStringLiteral("websites")] = QVariantList{ websiteEntry };
    fields[QStringLiteral("relations")] = QVariantList{ relationEntry };
    fields[QStringLiteral("events")] = QVariantList{ eventEntry };
    fields[QStringLiteral("phoneticGivenName")] = QStringLiteral("Ay-da");
    fields[QStringLiteral("phoneticFamilyName")] = QStringLiteral("Love-lace");
    fields[QStringLiteral("department")] = QStringLiteral("Engineering");
    fields[QStringLiteral("customFields")] = QVariantList{ customFieldEntry };
    fields[QStringLiteral("pronouns")] = QStringLiteral("she/her");

    const QString newUid = controller.createContact(fields);
    QVERIFY(!newUid.isEmpty());

    const QVariantMap created = controller.contactAt(newUid);
    QCOMPARE(created.value(QStringLiteral("groupIds")).toStringList(),
              QStringList({ QStringLiteral("group-1"), QStringLiteral("group-2") }));
    QCOMPARE(created.value(QStringLiteral("photoRef")).toString(), QStringLiteral("photo-ref-1"));
    QCOMPARE(created.value(QStringLiteral("pgpKey")).toString(), QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
    QCOMPARE(created.value(QStringLiteral("phoneticGivenName")).toString(), QStringLiteral("Ay-da"));
    QCOMPARE(created.value(QStringLiteral("phoneticFamilyName")).toString(), QStringLiteral("Love-lace"));
    QCOMPARE(created.value(QStringLiteral("department")).toString(), QStringLiteral("Engineering"));
    QCOMPARE(created.value(QStringLiteral("pronouns")).toString(), QStringLiteral("she/her"));

    const QVariantList createdIms = created.value(QStringLiteral("ims")).toList();
    QCOMPARE(createdIms.size(), 1);
    QCOMPARE(createdIms.at(0).toMap().value(QStringLiteral("service")).toString(), QStringLiteral("Matrix"));
    QCOMPARE(createdIms.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("@ada:example.org"));

    const QVariantList createdWebsites = created.value(QStringLiteral("websites")).toList();
    QCOMPARE(createdWebsites.size(), 1);
    QCOMPARE(createdWebsites.at(0).toMap().value(QStringLiteral("value")).toString(),
              QStringLiteral("https://ada.example.com"));

    const QVariantList createdRelations = created.value(QStringLiteral("relations")).toList();
    QCOMPARE(createdRelations.size(), 1);
    QCOMPARE(createdRelations.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("William King"));

    const QVariantList createdEvents = created.value(QStringLiteral("events")).toList();
    QCOMPARE(createdEvents.size(), 1);
    QCOMPARE(createdEvents.at(0).toMap().value(QStringLiteral("date")).toString(), QStringLiteral("2026-06-01"));

    const QVariantList createdCustomFields = created.value(QStringLiteral("customFields")).toList();
    QCOMPARE(createdCustomFields.size(), 1);
    QCOMPARE(createdCustomFields.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Employee ID"));
    QCOMPARE(createdCustomFields.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("42"));

    // updateContact is a whole-value/whole-list replace for every one of
    // these fields, same as it already is for org/notes -- omitting a key
    // from `fields` clears it, it does not preserve the prior value.
    QVariantMap updateFields;
    updateFields[QStringLiteral("fn")] = QStringLiteral("Ada Lovelace");
    updateFields[QStringLiteral("groupIds")] = QVariantList{ QStringLiteral("group-3") };
    updateFields[QStringLiteral("pronouns")] = QStringLiteral("they/them");
    // photoRef/pgpKey/ims/websites/relations/events/phoneticGivenName/
    // phoneticFamilyName/department/customFields deliberately omitted.

    QVERIFY(controller.updateContact(newUid, updateFields));

    const QVariantMap updated = controller.contactAt(newUid);
    QCOMPARE(updated.value(QStringLiteral("groupIds")).toStringList(), QStringList({ QStringLiteral("group-3") }));
    QCOMPARE(updated.value(QStringLiteral("pronouns")).toString(), QStringLiteral("they/them"));
    QCOMPARE(updated.value(QStringLiteral("photoRef")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("pgpKey")).toString(), QString());
    QVERIFY(updated.value(QStringLiteral("ims")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("websites")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("relations")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("events")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("customFields")).toList().isEmpty());
    QCOMPARE(updated.value(QStringLiteral("phoneticGivenName")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("phoneticFamilyName")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("department")).toString(), QString());
}

QTEST_GUILESS_MAIN(ContactsControllerTest)
#include "ContactsControllerTest.moc"
