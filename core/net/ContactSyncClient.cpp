#include "net/ContactSyncClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

// JSON mapping helpers -- kept here rather than in core/models/Contact.h so
// the plain model header stays free of wire-format concerns, matching how
// core/db/ContactDao.cpp already keeps SQL-mapping concerns out of the
// model too.

void putOptional(QJsonObject& obj, const QString& key, const std::optional<QString>& value)
{
    if (value)
        obj[key] = *value;
}

std::optional<QString> takeOptional(const QJsonObject& obj, const QString& key)
{
    if (!obj.contains(key) || obj.value(key).isNull())
        return std::nullopt;
    return obj.value(key).toString();
}

QJsonObject emailEntryToJson(const ContactEmailEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactEmailEntry emailEntryFromJson(const QJsonObject& obj)
{
    ContactEmailEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject phoneEntryToJson(const ContactPhoneEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactPhoneEntry phoneEntryFromJson(const QJsonObject& obj)
{
    ContactPhoneEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject addressEntryToJson(const ContactAddressEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    putOptional(obj, QStringLiteral("street"), entry.street);
    putOptional(obj, QStringLiteral("city"), entry.city);
    putOptional(obj, QStringLiteral("region"), entry.region);
    putOptional(obj, QStringLiteral("postalCode"), entry.postalCode);
    putOptional(obj, QStringLiteral("country"), entry.country);
    return obj;
}

ContactAddressEntry addressEntryFromJson(const QJsonObject& obj)
{
    ContactAddressEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.street = takeOptional(obj, QStringLiteral("street"));
    entry.city = takeOptional(obj, QStringLiteral("city"));
    entry.region = takeOptional(obj, QStringLiteral("region"));
    entry.postalCode = takeOptional(obj, QStringLiteral("postalCode"));
    entry.country = takeOptional(obj, QStringLiteral("country"));
    return entry;
}

QJsonObject imEntryToJson(const ContactImEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("service"), entry.service);
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactImEntry imEntryFromJson(const QJsonObject& obj)
{
    ContactImEntry entry;
    entry.service = takeOptional(obj, QStringLiteral("service"));
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject urlEntryToJson(const ContactUrlEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactUrlEntry urlEntryFromJson(const QJsonObject& obj)
{
    ContactUrlEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject relationEntryToJson(const ContactRelationEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("name")] = entry.name;
    return obj;
}

ContactRelationEntry relationEntryFromJson(const QJsonObject& obj)
{
    ContactRelationEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.name = obj.value(QStringLiteral("name")).toString();
    return entry;
}

QJsonObject eventEntryToJson(const ContactEventEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("date")] = entry.date;
    return obj;
}

ContactEventEntry eventEntryFromJson(const QJsonObject& obj)
{
    ContactEventEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.date = obj.value(QStringLiteral("date")).toString();
    return entry;
}

QJsonObject customFieldEntryToJson(const ContactCustomFieldEntry& entry)
{
    QJsonObject obj;
    obj[QStringLiteral("label")] = entry.label;
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactCustomFieldEntry customFieldEntryFromJson(const QJsonObject& obj)
{
    ContactCustomFieldEntry entry;
    entry.label = obj.value(QStringLiteral("label")).toString();
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

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
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("since"), QString::number(since) });

    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), query);

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

    const HttpClient::HttpResult result =
        m_httpClient.post(joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), auth.queryItems(), body);

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
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/dedupe")), auth.queryItems(), QJsonObject{});

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
