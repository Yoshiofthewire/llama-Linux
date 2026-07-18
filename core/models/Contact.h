#pragma once

#include <QString>
#include <QVector>
#include <optional>

struct ContactEmailEntry
{
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactEmailEntry&) const = default;
};

struct ContactPhoneEntry
{
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactPhoneEntry&) const = default;
};

struct ContactAddressEntry
{
    std::optional<QString> label;
    std::optional<QString> street;
    std::optional<QString> city;
    std::optional<QString> region;
    std::optional<QString> postalCode;
    std::optional<QString> country;

    bool operator==(const ContactAddressEntry&) const = default;
};

// IM handle -- e.g. {service: "Matrix", label: "work", value: "@ada:example.org"}.
// service is a free-text provider name (not an enum -- no fixed provider
// list is defined anywhere in this repo or the source doc).
struct ContactImEntry
{
    std::optional<QString> service;
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactImEntry&) const = default;
};

// Website/URL entry -- same shape as ContactEmailEntry.
struct ContactUrlEntry
{
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactUrlEntry&) const = default;
};

// Related person -- e.g. {label: "spouse", name: "Grace Hopper"}. name is
// a free-text display name, not a foreign key to another Contact.
struct ContactRelationEntry
{
    std::optional<QString> label;
    QString name;

    bool operator==(const ContactRelationEntry&) const = default;
};

// Non-birthday date -- e.g. {label: "anniversary", date: "2010-06-01"}.
// birthday itself stays the separate Contact::birthday field, unaffected.
struct ContactEventEntry
{
    std::optional<QString> label;
    QString date;

    bool operator==(const ContactEventEntry&) const = default;
};

// Free-form label/value pair. Unlike every other entry struct above, both
// fields are required (not optional) -- a custom field with no label isn't
// a meaningful entry.
struct ContactCustomFieldEntry
{
    QString label;
    QString value;

    bool operator==(const ContactCustomFieldEntry&) const = default;
};

struct Contact
{
    QString uid;
    qint64 rev = 0;
    std::optional<QString> createdAt;
    std::optional<QString> updatedAt;
    std::optional<QString> fn;
    std::optional<QString> givenName;
    std::optional<QString> familyName;
    std::optional<QString> middleName;
    std::optional<QString> prefix;
    std::optional<QString> suffix;
    std::optional<QString> nickname;
    std::optional<QString> org;
    std::optional<QString> title;
    std::optional<QString> notes;
    std::optional<QString> birthday;
    QVector<ContactEmailEntry> emails;
    QVector<ContactPhoneEntry> phones;
    QVector<ContactAddressEntry> addresses;
    QVector<QString> groupIds; // wire "groupIDs" (note the capitalization
                                // mismatch vs. this field name -- verbatim
                                // per the source doc's wire contract),
                                // SQLite "groups_json". Membership only;
                                // display names come from GET /api/groups
                                // (Task 2), not stored on Contact itself.
    std::optional<QString> photoRef; // opaque ref, not the photo bytes --
                                      // fetched separately via
                                      // GET /api/contacts/{id}/photo (Task 2)
    std::optional<QString> pgpKey;
    QVector<ContactImEntry> ims;
    QVector<ContactUrlEntry> websites;
    QVector<ContactRelationEntry> relations;
    QVector<ContactEventEntry> events;
    std::optional<QString> phoneticGivenName;
    std::optional<QString> phoneticFamilyName;
    std::optional<QString> department;
    QVector<ContactCustomFieldEntry> customFields;
    std::optional<QString> pronouns;
    bool deleted = false; // wire "deleted" (json:"deleted,omitempty" in the Go
                           // Contact struct) -- a push entry with {uid, rev,
                           // deleted:true} tombstones server-side, every
                           // other field omitted/zero.

    // Marks this as the caller's own contact card -- the (at most one,
    // enforced server-side) contact whose fields the server folds into the
    // PGP QR key-exchange response. Read-only on this client: the only
    // server endpoint that sets it requires a browser session cookie this
    // client doesn't have (see docs/superpowers/specs/
    // 2026-07-18-contact-self-and-pgp-qr-card-design.md), so this field is
    // only ever populated by a pull() and displayed, never set locally.
    bool isSelf = false;

    // Server-side-only dedupe provenance -- mergedUIDs lists the uids a
    // survivor absorbed, mergedInto points a loser's tombstone at the
    // survivor it was folded into. Never shown in any UI here; carried
    // through unchanged (never invented) purely so an edit-and-push of a
    // dedupe-survivor contact from this client stops silently wiping this
    // data server-side (Store.Upsert only special-cases CreatedAt/IsSelf on
    // update -- anything else the client's JSON omits gets zeroed).
    QVector<QString> mergedUIDs;
    std::optional<QString> mergedInto;

    bool operator==(const Contact&) const = default;
};
