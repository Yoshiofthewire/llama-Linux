#pragma once

#include "models/Contact.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>

// QML-facing read-only model wrapping a QVector<Contact> (Task 33), same
// pattern as EmailListModel (see app/mail/EmailListModel.h): owned by
// ContactsController, which is the only writer via setContacts(). One row
// per Contact. primaryEmail/primaryPhone are derived (first entry of
// emails/phones, empty string if none) rather than exposing the full
// nested-entry arrays as roles -- those are the two fields every list row
// and the Mac-style read-only card actually display; the full Contact
// struct (including every email/phone/address entry) is available to QML
// for the edit form via ContactsController::contactAt(uid) instead.
//
// synced role: `!pendingUids.contains(contact.uid)`, where pendingUids is
// supplied by ContactsController::load() from
// ContactSyncRepository::pendingUids() -- the real ground truth for "is
// there a queued create/update/delete for this uid that hasn't
// round-tripped through sync() yet" (see PendingContactChangeDao /
// ContactSyncRepository.cpp's queueCreate/queueUpdate/queueDelete, which
// enqueue there, and sync(), which clears the whole table on success).
// Previously this role used a `rev != 0` proxy on Contact itself, since
// queueCreate() assigns a temp local uid immediately (so `uid.isEmpty()`
// was never a usable test) and Contact had no direct synced/pending field.
// That proxy could not distinguish "queued update not yet pushed" from
// "successfully synced" and depended on the server never returning rev==0
// for a fresh object. Querying the pending table directly has neither
// limitation.
class ContactListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        UidRole = Qt::UserRole + 1,
        RevRole,
        FnRole,
        GivenNameRole,
        FamilyNameRole,
        OrgRole,
        NotesRole,
        PrimaryEmailRole,
        PrimaryPhoneRole,
        SyncedRole,
        // extended-contact-fields Task 3: exposes Contact::photoRef to
        // ContactsList.qml's row delegate so it can call
        // ContactsApp.photoPathFor(model.uid) for its Avatar -- deliberately
        // NOT added back in Task 1, which left this model untouched since
        // nothing consumed photoRef from a list row yet (see this class's
        // own doc comment: primaryEmail/primaryPhone are the only two
        // derived-from-nested-data roles this model exposes, everything
        // else is a scalar Contact field, and photoRef is the same shape as
        // those).
        PhotoRefRole,
    };

    explicit ContactListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // pendingUids marks rows with a queued-but-not-yet-synced change (see
    // this class's own doc comment above) -- pending state and the row set
    // itself always change together on a real ContactsController::load(),
    // so this is one call rather than a separate setter that could go
    // stale between the two.
    void setContacts(const QVector<Contact>& contacts, const QSet<QString>& pendingUids = {});
    Contact contactAt(int row) const; // out-of-range -> default-constructed Contact

private:
    QVector<Contact> m_contacts;
    QSet<QString> m_pendingUids;
};
