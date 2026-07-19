#include "net/ContactSyncClient.h"

#include "models/ContactFieldJson.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

// Per-entry toJson/fromJson mapping comes from models/ContactFieldJson.h,
// shared with core/db/ContactDao.cpp's SQLite-blob mapping -- kept out of
// core/models/Contact.h so the plain model header stays free of
// wire-format concerns.

// groupIds is a plain QVector<QString>, not a struct-entry list -- own
// encode/decode pair rather than going through entriesToJson/entriesFromJson.
QJsonArray stringListToJson(const QVector<QString>& values)
{
    QJsonArray array;
    for (const QString& value : values)
        array.append(value);
    return array;
}

QVector<QString> stringListFromJson(const QJsonArray& array)
{
    QVector<QString> values;
    values.reserve(array.size());
    for (const QJsonValue& value : array)
        values.append(value.toString());
    return values;
}

template <typename T, typename ToJsonFn>
QJsonArray entriesToJson(const QVector<T>& entries, ToJsonFn toJson)
{
    QJsonArray array;
    for (const T& entry : entries)
        array.append(toJson(entry));
    return array;
}

template <typename T, typename FromJsonFn>
QVector<T> entriesFromJson(const QJsonArray& array, FromJsonFn fromJson)
{
    QVector<T> entries;
    entries.reserve(array.size());
    for (const QJsonValue& value : array)
        entries.append(fromJson(value.toObject()));
    return entries;
}

} // namespace

namespace ContactWire {

// Every core/models/Contact.h field maps 1:1 onto a same-named JSON key
// (confirmed against the Go Contact/ContactValue/ContactAddress structs) --
// no field-name translation needed anywhere in this pair, with one
// exception: groupIds <-> "groupIDs" (verbatim per the wire contract).
QJsonObject contactToJson(const Contact& contact)
{
    QJsonObject obj;
    obj[QStringLiteral("uid")] = contact.uid;
    obj[QStringLiteral("rev")] = contact.rev;
    putOptional(obj, QStringLiteral("createdAt"), contact.createdAt);
    putOptional(obj, QStringLiteral("updatedAt"), contact.updatedAt);
    putOptional(obj, QStringLiteral("fn"), contact.fn);
    putOptional(obj, QStringLiteral("givenName"), contact.givenName);
    putOptional(obj, QStringLiteral("familyName"), contact.familyName);
    putOptional(obj, QStringLiteral("middleName"), contact.middleName);
    putOptional(obj, QStringLiteral("prefix"), contact.prefix);
    putOptional(obj, QStringLiteral("suffix"), contact.suffix);
    putOptional(obj, QStringLiteral("nickname"), contact.nickname);
    putOptional(obj, QStringLiteral("org"), contact.org);
    putOptional(obj, QStringLiteral("title"), contact.title);
    putOptional(obj, QStringLiteral("notes"), contact.notes);
    putOptional(obj, QStringLiteral("birthday"), contact.birthday);
    obj[QStringLiteral("emails")] = entriesToJson(contact.emails, emailEntryToJson);
    obj[QStringLiteral("phones")] = entriesToJson(contact.phones, phoneEntryToJson);
    obj[QStringLiteral("addresses")] = entriesToJson(contact.addresses, addressEntryToJson);
    // "groupIDs" -- verbatim capitalization per the wire contract, not
    // "groupIds" like the Contact.h field name.
    obj[QStringLiteral("groupIDs")] = stringListToJson(contact.groupIds);
    putOptional(obj, QStringLiteral("photoRef"), contact.photoRef);
    putOptional(obj, QStringLiteral("pgpKey"), contact.pgpKey);
    obj[QStringLiteral("ims")] = entriesToJson(contact.ims, imEntryToJson);
    obj[QStringLiteral("websites")] = entriesToJson(contact.websites, urlEntryToJson);
    obj[QStringLiteral("relations")] = entriesToJson(contact.relations, relationEntryToJson);
    obj[QStringLiteral("events")] = entriesToJson(contact.events, eventEntryToJson);
    putOptional(obj, QStringLiteral("phoneticGivenName"), contact.phoneticGivenName);
    putOptional(obj, QStringLiteral("phoneticFamilyName"), contact.phoneticFamilyName);
    putOptional(obj, QStringLiteral("department"), contact.department);
    obj[QStringLiteral("customFields")] = entriesToJson(contact.customFields, customFieldEntryToJson);
    putOptional(obj, QStringLiteral("pronouns"), contact.pronouns);
    if (contact.isSelf)
        obj[QStringLiteral("isSelf")] = true;
    obj[QStringLiteral("mergedUIDs")] = stringListToJson(contact.mergedUIDs);
    putOptional(obj, QStringLiteral("mergedInto"), contact.mergedInto);
    if (contact.deleted)
        obj[QStringLiteral("deleted")] = true;
    return obj;
}

Contact contactFromJson(const QJsonObject& obj)
{
    Contact contact;
    contact.uid = obj.value(QStringLiteral("uid")).toString();
    contact.rev = static_cast<qint64>(obj.value(QStringLiteral("rev")).toDouble());
    contact.createdAt = takeOptional(obj, QStringLiteral("createdAt"));
    contact.updatedAt = takeOptional(obj, QStringLiteral("updatedAt"));
    contact.fn = takeOptional(obj, QStringLiteral("fn"));
    contact.givenName = takeOptional(obj, QStringLiteral("givenName"));
    contact.familyName = takeOptional(obj, QStringLiteral("familyName"));
    contact.middleName = takeOptional(obj, QStringLiteral("middleName"));
    contact.prefix = takeOptional(obj, QStringLiteral("prefix"));
    contact.suffix = takeOptional(obj, QStringLiteral("suffix"));
    contact.nickname = takeOptional(obj, QStringLiteral("nickname"));
    contact.org = takeOptional(obj, QStringLiteral("org"));
    contact.title = takeOptional(obj, QStringLiteral("title"));
    contact.notes = takeOptional(obj, QStringLiteral("notes"));
    contact.birthday = takeOptional(obj, QStringLiteral("birthday"));
    contact.emails = entriesFromJson<ContactEmailEntry>(obj.value(QStringLiteral("emails")).toArray(), emailEntryFromJson);
    contact.phones = entriesFromJson<ContactPhoneEntry>(obj.value(QStringLiteral("phones")).toArray(), phoneEntryFromJson);
    contact.addresses =
        entriesFromJson<ContactAddressEntry>(obj.value(QStringLiteral("addresses")).toArray(), addressEntryFromJson);
    contact.groupIds = stringListFromJson(obj.value(QStringLiteral("groupIDs")).toArray());
    contact.photoRef = takeOptional(obj, QStringLiteral("photoRef"));
    contact.pgpKey = takeOptional(obj, QStringLiteral("pgpKey"));
    contact.ims = entriesFromJson<ContactImEntry>(obj.value(QStringLiteral("ims")).toArray(), imEntryFromJson);
    contact.websites =
        entriesFromJson<ContactUrlEntry>(obj.value(QStringLiteral("websites")).toArray(), urlEntryFromJson);
    contact.relations = entriesFromJson<ContactRelationEntry>(
        obj.value(QStringLiteral("relations")).toArray(), relationEntryFromJson);
    contact.events =
        entriesFromJson<ContactEventEntry>(obj.value(QStringLiteral("events")).toArray(), eventEntryFromJson);
    contact.phoneticGivenName = takeOptional(obj, QStringLiteral("phoneticGivenName"));
    contact.phoneticFamilyName = takeOptional(obj, QStringLiteral("phoneticFamilyName"));
    contact.department = takeOptional(obj, QStringLiteral("department"));
    contact.customFields = entriesFromJson<ContactCustomFieldEntry>(
        obj.value(QStringLiteral("customFields")).toArray(), customFieldEntryFromJson);
    contact.pronouns = takeOptional(obj, QStringLiteral("pronouns"));
    contact.isSelf = obj.value(QStringLiteral("isSelf")).toBool();
    contact.mergedUIDs = stringListFromJson(obj.value(QStringLiteral("mergedUIDs")).toArray());
    contact.mergedInto = takeOptional(obj, QStringLiteral("mergedInto"));
    contact.deleted = obj.value(QStringLiteral("deleted")).toBool();
    return contact;
}

} // namespace ContactWire

namespace {

// Parses the {cursor, tooOld, changed?, deleted?} response shape shared by
// both pull and push. changed/deleted are treated as empty (not a parse
// error) when absent -- the server omits both keys entirely when tooOld is
// true (writeContactsSyncResponse only sets them if !tooOld).
ContactSyncResult parseSyncResponse(const QJsonObject& json)
{
    ContactSyncResult out;
    out.cursor = static_cast<qint64>(json.value(QStringLiteral("cursor")).toDouble());
    out.tooOld = json.value(QStringLiteral("tooOld")).toBool();

    const QJsonArray changed = json.value(QStringLiteral("changed")).toArray();
    out.changed.reserve(changed.size());
    for (const QJsonValue& value : changed)
        out.changed.append(ContactWire::contactFromJson(value.toObject()));

    const QJsonArray deleted = json.value(QStringLiteral("deleted")).toArray();
    out.deletedContacts.reserve(deleted.size());
    for (const QJsonValue& value : deleted)
        out.deletedContacts.append(ContactWire::contactFromJson(value.toObject()));

    return out;
}

// Parses the {mergedCount, groups:[{survivor, absorbed[]}]} DedupeReport
// shape -- groups absent/empty maps to an empty vector, not a parse error,
// same "absent means empty" convention parseSyncResponse already follows.
ContactDedupeResult parseDedupeResponse(const QJsonObject& json)
{
    ContactDedupeResult out;
    out.mergedCount = json.value(QStringLiteral("mergedCount")).toInt();

    const QJsonArray groups = json.value(QStringLiteral("groups")).toArray();
    out.groups.reserve(groups.size());
    for (const QJsonValue& value : groups) {
        const QJsonObject groupObj = value.toObject();
        ContactDedupeGroup group;
        group.survivor = groupObj.value(QStringLiteral("survivor")).toString();
        const QJsonArray absorbed = groupObj.value(QStringLiteral("absorbed")).toArray();
        group.absorbed.reserve(absorbed.size());
        for (const QJsonValue& absorbedValue : absorbed)
            group.absorbed.append(absorbedValue.toString());
        out.groups.append(group);
    }

    return out;
}

} // namespace

ContactSyncClient::ContactSyncClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

ContactSyncResult ContactSyncClient::pull(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 since) const
{
    const QList<QPair<QString, QString>> query{ { QStringLiteral("since"), QString::number(since) } };

    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), query, auth.headerItems());

    ContactSyncResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact sync pull failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonObject> json = decodeJsonObject(result.body, &errorString);
    if (!json.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode contact sync response: %1").arg(errorString);
        return out;
    }

    return parseSyncResponse(*json);
}

ContactSyncResult ContactSyncClient::push(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 baseCursor,
                                           const QVector<Contact>& changes) const
{
    QJsonObject body;
    body[QStringLiteral("baseCursor")] = baseCursor;
    body[QStringLiteral("changes")] = entriesToJson(changes, ContactWire::contactToJson);

    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), {}, body, auth.headerItems());

    ContactSyncResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact sync push failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonObject> json = decodeJsonObject(result.body, &errorString);
    if (!json.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode contact sync response: %1").arg(errorString);
        return out;
    }

    return parseSyncResponse(*json);
}

ContactDedupeResult ContactSyncClient::dedupe(const QUrl& serverBaseUrl, const RelayAuth& auth) const
{
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/dedupe")), {}, QJsonObject{}, auth.headerItems());

    ContactDedupeResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact dedupe failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonObject> json = decodeJsonObject(result.body, &errorString);
    if (!json.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode contact dedupe response: %1").arg(errorString);
        return out;
    }

    return parseDedupeResponse(*json);
}
