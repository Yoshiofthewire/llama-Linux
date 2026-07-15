#pragma once

#include "domain/ContactSyncReconciliation.h"
#include "models/Contact.h"

#include <QString>
#include <QVector>
#include <optional>

class ContactSyncClient;
class ContactDao;
class PendingContactChangeDao;
class CursorStore;
class PairingStore;

struct ContactSyncSummary
{
    int pushed = 0;
    int applied = 0;
    qint64 newCursor = 0;

    bool operator==(const ContactSyncSummary&) const = default;
};

enum class ContactSyncStatus { Success, NotPaired, Unauthorized, ServiceUnavailable, Retry };

struct ContactSyncOutcome
{
    ContactSyncStatus status = ContactSyncStatus::Retry;
    ContactSyncSummary summary; // meaningful only when status == Success
    QString detail;             // meaningful when status != Success

    // Temp-uid -> real-uid pairs from this sync()'s reconciliation pass,
    // for callers (e.g. a native-contact link table) that must repoint
    // references away from a temp uid that's now dead in the local cache.
    // Empty on any early-return path (NotPaired, network error, tooOld).
    QVector<ContactReconciliationAssignment> uidReassignments;
};

// Sits between ContactSyncClient and ContactDao/PendingContactChangeDao,
// matching the Domain/Repositories layer in llama-Mail-for-Mac
// (ContactSyncRepository.swift) -- see ContactSyncReconciliation.h for the
// uid-assignment half of sync(). queueCreate/queueUpdate/queueDelete apply
// to the local cache immediately and enqueue the corresponding wire change
// for the next sync() call; sync() itself either pushes the queue (if
// non-empty) or pulls (if empty), then merges the response back into the
// cache -- see sync()'s .cpp comments for the partial-delta merge rule.
class ContactSyncRepository
{
public:
    ContactSyncRepository(ContactSyncClient& client, ContactDao& contactDao,
                           PendingContactChangeDao& pendingDao, CursorStore& cursorStore,
                           PairingStore& pairingStore);

    QVector<Contact> contacts() const; // contactDao.findAll()

    std::optional<Contact> findByUid(const QString& uid) const; // contactDao.findById()

    // Assigns a temp local uid (QUuid::createUuid().toString(QUuid::
    // WithoutBraces)), caches it under that uid immediately, and enqueues a
    // create (wire copy has uid=="", per ContactSyncClient's documented
    // "empty uid marks a create" contract) for the next sync().
    QString queueCreate(Contact contact);

    // contact.uid must already be a real server uid. Updates the cache
    // immediately and enqueues the wire copy as-is (uid+rev present,
    // deleted=false) for the next sync().
    void queueUpdate(const Contact& contact);

    // Removes the row from the local cache immediately. Enqueues a
    // tombstone ({uid, rev, deleted=true}, every other field default) for
    // the next sync() -- UNLESS uid refers to a contact that was never
    // synced (i.e. there is a still-pending queued create for this same uid
    // and nothing else): in that case, the pending create row is deleted
    // outright instead of enqueuing a delete, since the server never saw it.
    void queueDelete(const QString& uid, qint64 rev);

    ContactSyncOutcome sync();

private:
    ContactSyncClient& m_client;
    ContactDao& m_contactDao;
    PendingContactChangeDao& m_pendingDao;
    CursorStore& m_cursorStore;
    PairingStore& m_pairingStore;
};
