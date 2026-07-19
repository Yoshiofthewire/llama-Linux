#include "net/ContactSyncClient.h"

#include "models/Contact.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTest>

class ContactSyncClientTest : public QObject
{
    Q_OBJECT

private slots:
    void pullRoundTripMapsPopulatedAndAbsentOptionalFieldsIncludingNestedEntries();
    void pullSendsSinceAsQueryParamAndAuthAsHeaders();
    void pushRoundTripSendsExactFieldNamesIncludingEmptyUidCreate();
    void pushSendsAuthAsHeaders();
    void tooOldTrueSurfacesFlagWithEmptyChangedAndDeleted();
    void pullUnauthorizedFrom401PassesErrorThrough();
    void deletedFieldRoundTripsTrueAndOmittedWhenFalse();
    void dedupeParsesReportIntoMergedCountAndGroups();
    void dedupeSendsAuthAsHeadersAndPostsToApiContactsDedupe();
    void dedupeOnEmptyGroupsReturnsZeroMergedCountNoError();
    void dedupeUnauthorizedFrom401MapsToError();
    void dedupeOnMalformedBodyReturnsDecodingErrorNotCrash();
};

namespace {

// Contact 1: every optional scalar field populated, plus one nested entry
// of each kind (all sub-fields populated).
const QByteArray kPopulatedContactJson = R"(
{
  "uid": "c-1",
  "rev": 5,
  "createdAt": "2026-01-01T00:00:00Z",
  "updatedAt": "2026-02-01T00:00:00Z",
  "fn": "Ada Lovelace",
  "givenName": "Ada",
  "familyName": "Lovelace",
  "middleName": "Augusta",
  "prefix": "Countess",
  "suffix": "Esq.",
  "nickname": "Ada",
  "org": "Analytical Engines Ltd",
  "title": "Mathematician",
  "notes": "Pioneer of computing",
  "birthday": "1815-12-10",
  "emails": [{"label":"work","value":"ada@example.com"}],
  "phones": [{"label":"mobile","value":"+1-555-0100"}],
  "addresses": [{"label":"home","street":"1 Main St","city":"London","region":"London","postalCode":"SW1A 1AA","country":"UK"}],
  "groupIDs": ["group-1", "group-2"],
  "photoRef": "photo-ref-1",
  "pgpKey": "-----BEGIN PGP PUBLIC KEY BLOCK-----",
  "ims": [{"service":"Matrix","label":"work","value":"@ada:example.org"}],
  "websites": [{"label":"blog","value":"https://ada.example.com"}],
  "relations": [{"label":"spouse","name":"William King"}],
  "events": [{"label":"anniversary","date":"2026-06-01"}],
  "phoneticGivenName": "Ay-da",
  "phoneticFamilyName": "Love-lace",
  "department": "Engineering",
  "customFields": [{"label":"Employee ID","value":"42"}],
  "pronouns": "she/her",
  "isSelf": true,
  "mergedUIDs": ["merged-1", "merged-2"],
  "mergedInto": "survivor-uid"
}
)";

// Contact 2: only the required fields (uid, rev) present -- every optional
// scalar field and every array key entirely absent from the wire.
const QByteArray kMinimalContactJson = R"(
{
  "uid": "c-2",
  "rev": 1
}
)";

// Deleted-array entry: full Contact JSON per the resolved wire contract
// (contacts.Store.ChangedSince returns full Contact structs, tombstoned),
// not a bare uid.
const QByteArray kDeletedContactJson = R"(
{
  "uid": "c-3",
  "rev": 9,
  "emails": [{"value":"nolabel@example.com"}]
}
)";

} // namespace

void ContactSyncClientTest::pullRoundTripMapsPopulatedAndAbsentOptionalFieldsIncludingNestedEntries()
{
    const QByteArray body = "{\"cursor\":100,\"tooOld\":false,\"changed\":[" + kPopulatedContactJson + ","
        + kMinimalContactJson + "],\"deleted\":[" + kDeletedContactJson + "]}";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactSyncResult result = client.pull(serverBaseUrl, auth, 0);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.cursor, qint64(100));
    QCOMPARE(result.tooOld, false);
    QCOMPARE(result.changed.size(), 2);
    QCOMPARE(result.deletedContacts.size(), 1);

    // Contact 1: every field, including nested entries, maps exactly.
    const Contact& c1 = result.changed.at(0);
    QCOMPARE(c1.uid, QStringLiteral("c-1"));
    QCOMPARE(c1.rev, qint64(5));
    QVERIFY(c1.createdAt.has_value());
    QCOMPARE(*c1.createdAt, QStringLiteral("2026-01-01T00:00:00Z"));
    QVERIFY(c1.updatedAt.has_value());
    QCOMPARE(*c1.updatedAt, QStringLiteral("2026-02-01T00:00:00Z"));
    QVERIFY(c1.fn.has_value());
    QCOMPARE(*c1.fn, QStringLiteral("Ada Lovelace"));
    QVERIFY(c1.givenName.has_value());
    QCOMPARE(*c1.givenName, QStringLiteral("Ada"));
    QVERIFY(c1.familyName.has_value());
    QCOMPARE(*c1.familyName, QStringLiteral("Lovelace"));
    QVERIFY(c1.middleName.has_value());
    QCOMPARE(*c1.middleName, QStringLiteral("Augusta"));
    QVERIFY(c1.prefix.has_value());
    QCOMPARE(*c1.prefix, QStringLiteral("Countess"));
    QVERIFY(c1.suffix.has_value());
    QCOMPARE(*c1.suffix, QStringLiteral("Esq."));
    QVERIFY(c1.nickname.has_value());
    QCOMPARE(*c1.nickname, QStringLiteral("Ada"));
    QVERIFY(c1.org.has_value());
    QCOMPARE(*c1.org, QStringLiteral("Analytical Engines Ltd"));
    QVERIFY(c1.title.has_value());
    QCOMPARE(*c1.title, QStringLiteral("Mathematician"));
    QVERIFY(c1.notes.has_value());
    QCOMPARE(*c1.notes, QStringLiteral("Pioneer of computing"));
    QVERIFY(c1.birthday.has_value());
    QCOMPARE(*c1.birthday, QStringLiteral("1815-12-10"));

    QCOMPARE(c1.emails.size(), 1);
    QVERIFY(c1.emails.at(0).label.has_value());
    QCOMPARE(*c1.emails.at(0).label, QStringLiteral("work"));
    QCOMPARE(c1.emails.at(0).value, QStringLiteral("ada@example.com"));

    QCOMPARE(c1.phones.size(), 1);
    QVERIFY(c1.phones.at(0).label.has_value());
    QCOMPARE(*c1.phones.at(0).label, QStringLiteral("mobile"));
    QCOMPARE(c1.phones.at(0).value, QStringLiteral("+1-555-0100"));

    QCOMPARE(c1.addresses.size(), 1);
    const ContactAddressEntry& addr = c1.addresses.at(0);
    QVERIFY(addr.label.has_value());
    QCOMPARE(*addr.label, QStringLiteral("home"));
    QVERIFY(addr.street.has_value());
    QCOMPARE(*addr.street, QStringLiteral("1 Main St"));
    QVERIFY(addr.city.has_value());
    QCOMPARE(*addr.city, QStringLiteral("London"));
    QVERIFY(addr.region.has_value());
    QCOMPARE(*addr.region, QStringLiteral("London"));
    QVERIFY(addr.postalCode.has_value());
    QCOMPARE(*addr.postalCode, QStringLiteral("SW1A 1AA"));
    QVERIFY(addr.country.has_value());
    QCOMPARE(*addr.country, QStringLiteral("UK"));

    QCOMPARE(c1.groupIds, (QVector<QString>{QStringLiteral("group-1"), QStringLiteral("group-2")}));
    QVERIFY(c1.photoRef.has_value());
    QCOMPARE(*c1.photoRef, QStringLiteral("photo-ref-1"));
    QVERIFY(c1.pgpKey.has_value());
    QCOMPARE(*c1.pgpKey, QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));

    QCOMPARE(c1.ims.size(), 1);
    QVERIFY(c1.ims.at(0).service.has_value());
    QCOMPARE(*c1.ims.at(0).service, QStringLiteral("Matrix"));
    QVERIFY(c1.ims.at(0).label.has_value());
    QCOMPARE(*c1.ims.at(0).label, QStringLiteral("work"));
    QCOMPARE(c1.ims.at(0).value, QStringLiteral("@ada:example.org"));

    QCOMPARE(c1.websites.size(), 1);
    QVERIFY(c1.websites.at(0).label.has_value());
    QCOMPARE(*c1.websites.at(0).label, QStringLiteral("blog"));
    QCOMPARE(c1.websites.at(0).value, QStringLiteral("https://ada.example.com"));

    QCOMPARE(c1.relations.size(), 1);
    QVERIFY(c1.relations.at(0).label.has_value());
    QCOMPARE(*c1.relations.at(0).label, QStringLiteral("spouse"));
    QCOMPARE(c1.relations.at(0).name, QStringLiteral("William King"));

    QCOMPARE(c1.events.size(), 1);
    QVERIFY(c1.events.at(0).label.has_value());
    QCOMPARE(*c1.events.at(0).label, QStringLiteral("anniversary"));
    QCOMPARE(c1.events.at(0).date, QStringLiteral("2026-06-01"));

    QVERIFY(c1.phoneticGivenName.has_value());
    QCOMPARE(*c1.phoneticGivenName, QStringLiteral("Ay-da"));
    QVERIFY(c1.phoneticFamilyName.has_value());
    QCOMPARE(*c1.phoneticFamilyName, QStringLiteral("Love-lace"));
    QVERIFY(c1.department.has_value());
    QCOMPARE(*c1.department, QStringLiteral("Engineering"));

    QCOMPARE(c1.customFields.size(), 1);
    QCOMPARE(c1.customFields.at(0).label, QStringLiteral("Employee ID"));
    QCOMPARE(c1.customFields.at(0).value, QStringLiteral("42"));

    QVERIFY(c1.pronouns.has_value());
    QCOMPARE(*c1.pronouns, QStringLiteral("she/her"));

    QCOMPARE(c1.isSelf, true);
    QCOMPARE(c1.mergedUIDs, QVector<QString>({QStringLiteral("merged-1"), QStringLiteral("merged-2")}));
    QCOMPARE(c1.mergedInto, std::optional<QString>(QStringLiteral("survivor-uid")));

    // Contact 2: every optional field and every array key absent from the
    // wire maps to nullopt / empty vectors, not a parse error.
    const Contact& c2 = result.changed.at(1);
    QCOMPARE(c2.uid, QStringLiteral("c-2"));
    QCOMPARE(c2.rev, qint64(1));
    QVERIFY(!c2.createdAt.has_value());
    QVERIFY(!c2.updatedAt.has_value());
    QVERIFY(!c2.fn.has_value());
    QVERIFY(!c2.givenName.has_value());
    QVERIFY(!c2.familyName.has_value());
    QVERIFY(!c2.middleName.has_value());
    QVERIFY(!c2.prefix.has_value());
    QVERIFY(!c2.suffix.has_value());
    QVERIFY(!c2.nickname.has_value());
    QVERIFY(!c2.org.has_value());
    QVERIFY(!c2.title.has_value());
    QVERIFY(!c2.notes.has_value());
    QVERIFY(!c2.birthday.has_value());
    QVERIFY(c2.emails.isEmpty());
    QVERIFY(c2.phones.isEmpty());
    QVERIFY(c2.addresses.isEmpty());
    QVERIFY(c2.groupIds.isEmpty());
    QVERIFY(!c2.photoRef.has_value());
    QVERIFY(!c2.pgpKey.has_value());
    QVERIFY(c2.ims.isEmpty());
    QVERIFY(c2.websites.isEmpty());
    QVERIFY(c2.relations.isEmpty());
    QVERIFY(c2.events.isEmpty());
    QVERIFY(!c2.phoneticGivenName.has_value());
    QVERIFY(!c2.phoneticFamilyName.has_value());
    QVERIFY(!c2.department.has_value());
    QVERIFY(c2.customFields.isEmpty());
    QVERIFY(!c2.pronouns.has_value());

    QCOMPARE(c2.isSelf, false);
    QVERIFY(c2.mergedUIDs.isEmpty());
    QVERIFY(!c2.mergedInto.has_value());

    // deletedContacts holds full Contact objects (not bare uids) -- assert
    // fields, including a nested entry with an absent label.
    const Contact& deleted = result.deletedContacts.at(0);
    QCOMPARE(deleted.uid, QStringLiteral("c-3"));
    QCOMPARE(deleted.rev, qint64(9));
    QCOMPARE(deleted.emails.size(), 1);
    QVERIFY(!deleted.emails.at(0).label.has_value());
    QCOMPARE(deleted.emails.at(0).value, QStringLiteral("nolabel@example.com"));
}

void ContactSyncClientTest::pullSendsSinceAsQueryParamAndAuthAsHeaders()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":0,"tooOld":false,"changed":[],"deleted":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.pull(serverBaseUrl, auth, 0);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/contacts/sync?"));
    QVERIFY(request.contains("since=0"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}

void ContactSyncClientTest::pushRoundTripSendsExactFieldNamesIncludingEmptyUidCreate()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":101,"tooOld":false,"changed":[],"deleted":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    Contact created; // empty uid marks a create
    created.rev = 0;
    created.fn = QStringLiteral("New Contact");
    created.emails = { ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("new@example.com") } };

    Contact updated;
    updated.uid = QStringLiteral("c-existing");
    updated.rev = 3;
    updated.givenName = QStringLiteral("Grace");
    updated.addresses = { ContactAddressEntry{ std::nullopt, QStringLiteral("42 Wallaby Way"), std::nullopt,
                                                std::nullopt, std::nullopt, std::nullopt } };
    updated.groupIds = { QStringLiteral("group-9") };
    updated.pronouns = QStringLiteral("they/them");
    updated.isSelf = true;
    updated.mergedUIDs = {QStringLiteral("merged-1")};
    updated.mergedInto = QStringLiteral("survivor-uid");

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactSyncResult result = client.push(serverBaseUrl, auth, 50, { created, updated });

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.cursor, qint64(101));

    QVERIFY(fake.receivedRequest().contains("POST /api/contacts/sync HTTP/1.1"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("baseCursor")).toInt(), 50);
    QVERIFY(sent.contains(QStringLiteral("changes")));
    const QJsonArray changes = sent.value(QStringLiteral("changes")).toArray();
    QCOMPARE(changes.size(), 2);

    const QJsonObject sentCreated = changes.at(0).toObject();
    QCOMPARE(sentCreated.value(QStringLiteral("uid")).toString(), QString());
    QVERIFY(sentCreated.contains(QStringLiteral("uid")));
    QCOMPARE(sentCreated.value(QStringLiteral("rev")).toInt(), 0);
    QCOMPARE(sentCreated.value(QStringLiteral("fn")).toString(), QStringLiteral("New Contact"));
    QCOMPARE(sentCreated.value(QStringLiteral("emails")).toArray().size(), 1);
    const QJsonObject sentEmail = sentCreated.value(QStringLiteral("emails")).toArray().at(0).toObject();
    QCOMPARE(sentEmail.value(QStringLiteral("label")).toString(), QStringLiteral("work"));
    QCOMPARE(sentEmail.value(QStringLiteral("value")).toString(), QStringLiteral("new@example.com"));
    // Optional fields that were never set must not appear on the wire.
    QVERIFY(!sentCreated.contains(QStringLiteral("createdAt")));
    QVERIFY(!sentCreated.contains(QStringLiteral("givenName")));
    QVERIFY(!sentCreated.contains(QStringLiteral("birthday")));
    QVERIFY(!sentCreated.contains(QStringLiteral("photoRef")));
    QVERIFY(!sentCreated.contains(QStringLiteral("pgpKey")));
    QVERIFY(!sentCreated.contains(QStringLiteral("phoneticGivenName")));
    QVERIFY(!sentCreated.contains(QStringLiteral("phoneticFamilyName")));
    QVERIFY(!sentCreated.contains(QStringLiteral("department")));
    QVERIFY(!sentCreated.contains(QStringLiteral("pronouns")));
    // List fields are always present, even when empty (same convention as
    // emails/phones/addresses), using the wire's "groupIDs" spelling.
    QVERIFY(sentCreated.value(QStringLiteral("groupIDs")).toArray().isEmpty());
    QVERIFY(sentCreated.value(QStringLiteral("ims")).toArray().isEmpty());
    QVERIFY(sentCreated.value(QStringLiteral("websites")).toArray().isEmpty());
    QVERIFY(sentCreated.value(QStringLiteral("relations")).toArray().isEmpty());
    QVERIFY(sentCreated.value(QStringLiteral("events")).toArray().isEmpty());
    QVERIFY(sentCreated.value(QStringLiteral("customFields")).toArray().isEmpty());

    const QJsonObject sentUpdated = changes.at(1).toObject();
    QCOMPARE(sentUpdated.value(QStringLiteral("uid")).toString(), QStringLiteral("c-existing"));
    QCOMPARE(sentUpdated.value(QStringLiteral("rev")).toInt(), 3);
    QCOMPARE(sentUpdated.value(QStringLiteral("givenName")).toString(), QStringLiteral("Grace"));
    QCOMPARE(sentUpdated.value(QStringLiteral("addresses")).toArray().size(), 1);
    const QJsonObject sentAddress = sentUpdated.value(QStringLiteral("addresses")).toArray().at(0).toObject();
    QCOMPARE(sentAddress.value(QStringLiteral("street")).toString(), QStringLiteral("42 Wallaby Way"));
    QVERIFY(!sentAddress.contains(QStringLiteral("label")));
    QVERIFY(!sentAddress.contains(QStringLiteral("city")));

    const QJsonArray sentGroupIds = sentUpdated.value(QStringLiteral("groupIDs")).toArray();
    QCOMPARE(sentGroupIds.size(), 1);
    QCOMPARE(sentGroupIds.at(0).toString(), QStringLiteral("group-9"));
    QCOMPARE(sentUpdated.value(QStringLiteral("pronouns")).toString(), QStringLiteral("they/them"));

    QCOMPARE(sentUpdated.value(QStringLiteral("isSelf")).toBool(), true);
    QCOMPARE(sentUpdated.value(QStringLiteral("mergedUIDs")).toArray().size(), 1);
    QCOMPARE(sentUpdated.value(QStringLiteral("mergedUIDs")).toArray().first().toString(), QStringLiteral("merged-1"));
    QCOMPARE(sentUpdated.value(QStringLiteral("mergedInto")).toString(), QStringLiteral("survivor-uid"));
}

void ContactSyncClientTest::pushSendsAuthAsHeaders()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":1,"tooOld":false,"changed":[],"deleted":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-7"), QStringLiteral("hash-7") };
    client.push(serverBaseUrl, auth, 0, {});

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-7"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-7"));
    QVERIFY(!request.contains("sub=sub-7"));
    QVERIFY(!request.contains("hash=hash-7"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("baseCursor")).toInt(), 0);
    QVERIFY(sent.value(QStringLiteral("changes")).toArray().isEmpty());
}

void ContactSyncClientTest::tooOldTrueSurfacesFlagWithEmptyChangedAndDeleted()
{
    // The server omits "changed"/"deleted" entirely when tooOld is true
    // (writeContactsSyncResponse only sets those keys if !tooOld) -- the
    // client must treat that as empty vectors, not a parse error, and must
    // not attempt any reset/wipe logic itself (that's Phase 4).
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":200,"tooOld":true})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactSyncResult result = client.pull(serverBaseUrl, auth, 999999);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.tooOld, true);
    QCOMPARE(result.cursor, qint64(200));
    QVERIFY(result.changed.isEmpty());
    QVERIFY(result.deletedContacts.isEmpty());
}

void ContactSyncClientTest::pullUnauthorizedFrom401PassesErrorThrough()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactSyncResult result = client.pull(serverBaseUrl, auth, 0);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.changed.isEmpty());
    QVERIFY(result.deletedContacts.isEmpty());
}

void ContactSyncClientTest::deletedFieldRoundTripsTrueAndOmittedWhenFalse()
{
    Contact tombstone;
    tombstone.uid = QStringLiteral("c-9");
    tombstone.rev = 2;
    tombstone.deleted = true;
    const QJsonObject tombstoneJson = ContactWire::contactToJson(tombstone);
    QVERIFY(tombstoneJson.contains(QStringLiteral("deleted")));
    QCOMPARE(tombstoneJson.value(QStringLiteral("deleted")).toBool(), true);

    const Contact roundTripped = ContactWire::contactFromJson(tombstoneJson);
    QCOMPARE(roundTripped.deleted, true);

    Contact notDeleted;
    notDeleted.uid = QStringLiteral("c-10");
    const QJsonObject notDeletedJson = ContactWire::contactToJson(notDeleted);
    QVERIFY(!notDeletedJson.contains(QStringLiteral("deleted")));

    const Contact roundTrippedNotDeleted = ContactWire::contactFromJson(notDeletedJson);
    QCOMPARE(roundTrippedNotDeleted.deleted, false);
}

void ContactSyncClientTest::dedupeParsesReportIntoMergedCountAndGroups()
{
    const QByteArray body =
        R"({"mergedCount":3,"groups":[{"survivor":"c-1","absorbed":["c-2","c-3"]},{"survivor":"c-4","absorbed":["c-5"]}]})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactDedupeResult result = client.dedupe(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.mergedCount, 3);
    QCOMPARE(result.groups.size(), 2);
    QCOMPARE(result.groups.at(0).survivor, QStringLiteral("c-1"));
    QCOMPARE(result.groups.at(0).absorbed, (QVector<QString>{QStringLiteral("c-2"), QStringLiteral("c-3")}));
    QCOMPARE(result.groups.at(1).survivor, QStringLiteral("c-4"));
    QCOMPARE(result.groups.at(1).absorbed, (QVector<QString>{QStringLiteral("c-5")}));
}

void ContactSyncClientTest::dedupeSendsAuthAsHeadersAndPostsToApiContactsDedupe()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"mergedCount":0,"groups":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.dedupe(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/contacts/dedupe HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}

void ContactSyncClientTest::dedupeOnEmptyGroupsReturnsZeroMergedCountNoError()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"mergedCount":0,"groups":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactDedupeResult result = client.dedupe(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.mergedCount, 0);
    QVERIFY(result.groups.isEmpty());
}

void ContactSyncClientTest::dedupeUnauthorizedFrom401MapsToError()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactDedupeResult result = client.dedupe(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QCOMPARE(result.mergedCount, 0);
    QVERIFY(result.groups.isEmpty());
}

void ContactSyncClientTest::dedupeOnMalformedBodyReturnsDecodingErrorNotCrash()
{
    FakeRelayServer fake(httpResponse(200, "OK", "not json"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactDedupeResult result = client.dedupe(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Decoding);
}

QTEST_GUILESS_MAIN(ContactSyncClientTest)
#include "ContactSyncClientTest.moc"
