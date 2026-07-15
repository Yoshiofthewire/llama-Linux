#include "contacts/native/NativeContactSyncRepository.h"

#include "db/NativeContactLinkDao.h"
#include "domain/ContactSyncRepository.h"
#include "domain/NativeContactReconciliation.h"
#include "vcard/VCardContact.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>

namespace {

QString hashVCard(const QString& vcard)
{
    return QString::fromLatin1(QCryptographicHash::hash(vcard.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace

NativeContactSyncRepository::NativeContactSyncRepository(NativeContactsProvider& provider,
                                                           ContactSyncRepository& contactRepo,
                                                           NativeContactLinkDao& linkDao)
    : m_provider(provider)
    , m_contactRepo(contactRepo)
    , m_linkDao(linkDao)
{
}

NativeContactSyncOutcome NativeContactSyncRepository::sync()
{
    NativeContactSyncOutcome outcome;
    outcome.availability = m_provider.probeAvailability();
    if (outcome.availability != NativeContactsAvailability::Available)
        return outcome; // short-circuit -- no listItems/fetchVCard/create/update/delete calls at all

    const QString backendId = m_provider.backendId();
    const bool neverLinked = m_linkDao.findAllForBackend(backendId).isEmpty();
    return neverLinked ? firstEnableSync(backendId) : steadyStateSync(backendId);
}

NativeContactSyncOutcome NativeContactSyncRepository::firstEnableSync(const QString& backendId)
{
    NativeContactSyncOutcome outcome;
    outcome.availability = NativeContactsAvailability::Available;

    // Fetch every native item's vCard up front -- matchUnlinked needs parsed
    // Contact content to compare, but the raw text is also needed later (for
    // hashing and for the local-import path), so both are kept side by side
    // keyed by nativeItemId.
    QHash<QString, QString> nativeVCardByItemId;
    QHash<QString, QString> nativeSourceIdByItemId;
    QVector<QPair<QString, Contact>> nativeContacts;
    for (const NativeContactSummary& summary : m_provider.listItems()) {
        const std::optional<QString> vcard = m_provider.fetchVCard(summary.nativeItemId);
        if (!vcard)
            continue;
        nativeVCardByItemId.insert(summary.nativeItemId, *vcard);
        nativeSourceIdByItemId.insert(summary.nativeItemId, summary.nativeSourceId);
        nativeContacts.append({ summary.nativeItemId, VCardContact::contactFromVCard(*vcard) });
    }

    // No link row exists yet for this backend, so by definition every local
    // contact is currently unlinked.
    const QVector<Contact> unlinkedLocal = m_contactRepo.contacts();
    QHash<QString, Contact> localByUid;
    for (const Contact& c : unlinkedLocal)
        localByUid.insert(c.uid, c);

    const NativeMatchResult matchResult = NativeContactReconciliation::matchUnlinked(unlinkedLocal, nativeContacts);
    const QString syncedAt = nowIso();

    for (const auto& pair : matchResult.matched) {
        const QString& localUid = pair.first;
        const QString& nativeItemId = pair.second;
        const Contact& local = localByUid.value(localUid);

        // Matched pairs are linked as-is, with no push/pull -- matching is a
        // content heuristic (see NativeContactReconciliation), not a
        // guarantee the two sides' vCard text is byte-identical. The hash
        // recorded here is the local side's; if the native side's text
        // genuinely differs, the very next steady-state sync() will surface
        // that as an ordinary push/pull/conflict, same as any later drift --
        // deliberately not treated as special-case logic here.
        NativeContactLink link;
        link.localUid = localUid;
        link.backend = backendId;
        link.nativeItemId = nativeItemId;
        link.nativeSourceId = nativeSourceIdByItemId.value(nativeItemId);
        link.lastSyncedHash = hashVCard(VCardContact::contactToVCard(local));
        link.lastSyncedAt = syncedAt;
        m_linkDao.insertOrReplace(link);
    }

    const std::optional<QString> defaultSourceId = m_provider.defaultSourceId();
    for (const QString& localUid : matchResult.unmatchedLocalUids) {
        if (!defaultSourceId)
            continue; // nowhere to create it on native -- left unlinked, retried on the next first-enable pass

        const Contact& local = localByUid.value(localUid);
        const QString vcard = VCardContact::contactToVCard(local);
        const std::optional<QString> nativeItemId = m_provider.createItem(*defaultSourceId, vcard);
        if (!nativeItemId)
            continue;

        NativeContactLink link;
        link.localUid = localUid;
        link.backend = backendId;
        link.nativeItemId = *nativeItemId;
        link.nativeSourceId = *defaultSourceId;
        link.lastSyncedHash = hashVCard(vcard);
        link.lastSyncedAt = syncedAt;
        m_linkDao.insertOrReplace(link);
        ++outcome.createdOnNative;
    }

    for (const QString& nativeItemId : matchResult.unmatchedNativeItemIds) {
        const QString vcard = nativeVCardByItemId.value(nativeItemId);
        const QString tempUid = m_contactRepo.queueCreate(VCardContact::contactFromVCard(vcard));

        NativeContactLink link;
        link.localUid = tempUid;
        link.backend = backendId;
        link.nativeItemId = nativeItemId;
        link.nativeSourceId = nativeSourceIdByItemId.value(nativeItemId);
        link.lastSyncedHash = hashVCard(vcard);
        link.lastSyncedAt = syncedAt;
        m_linkDao.insertOrReplace(link);
        ++outcome.createdLocally;
    }

    return outcome;
}

NativeContactSyncOutcome NativeContactSyncRepository::steadyStateSync(const QString& backendId)
{
    NativeContactSyncOutcome outcome;
    outcome.availability = NativeContactsAvailability::Available;

    // modifiedAt only comes back via listItems()'s summaries (fetchVCard
    // doesn't carry it), and is only needed for the Conflict tiebreak below
    // -- listed once per sync() pass rather than once per link.
    QHash<QString, std::optional<QDateTime>> modifiedAtByItemId;
    for (const NativeContactSummary& summary : m_provider.listItems())
        modifiedAtByItemId.insert(summary.nativeItemId, summary.modifiedAt);

    const QString syncedAt = nowIso();

    for (const NativeContactLink& link : m_linkDao.findAllForBackend(backendId)) {
        const std::optional<Contact> localContact = m_contactRepo.findByUid(link.localUid);
        const std::optional<QString> nativeVCard = m_provider.fetchVCard(link.nativeItemId);

        if (!nativeVCard) {
            // Native-side deletion: the item is no longer fetchable/listed.
            // Hash-diffing alone can't distinguish "unchanged" from "gone",
            // so absence must be checked before any hash comparison.
            if (localContact)
                m_contactRepo.queueDelete(localContact->uid, localContact->rev);
            m_linkDao.deleteByLocalUid(link.localUid, backendId);
            continue;
        }

        if (!localContact) {
            // Local-side deletion: the uid is no longer in the local cache.
            m_provider.deleteItem(link.nativeItemId);
            m_linkDao.deleteByLocalUid(link.localUid, backendId);
            continue;
        }

        const QString localHash = hashVCard(VCardContact::contactToVCard(*localContact));
        const QString nativeHash = hashVCard(*nativeVCard);
        const NativeContactReconciliation::SyncAction action =
            NativeContactReconciliation::decide(link.lastSyncedHash, localHash, nativeHash);

        if (action == NativeContactReconciliation::SyncAction::NoChange)
            continue;

        if (action == NativeContactReconciliation::SyncAction::PushLocalToNative) {
            m_provider.updateItem(link.nativeItemId, VCardContact::contactToVCard(*localContact));
            NativeContactLink updatedLink = link;
            updatedLink.lastSyncedHash = localHash;
            updatedLink.lastSyncedAt = syncedAt;
            m_linkDao.insertOrReplace(updatedLink);
            ++outcome.pushedToNative;
            continue;
        }

        if (action == NativeContactReconciliation::SyncAction::PullNativeToLocal) {
            Contact updated = VCardContact::contactFromVCard(*nativeVCard);
            updated.uid = localContact->uid;
            updated.rev = localContact->rev;
            m_contactRepo.queueUpdate(updated);
            NativeContactLink updatedLink = link;
            updatedLink.lastSyncedHash = nativeHash;
            updatedLink.lastSyncedAt = syncedAt;
            m_linkDao.insertOrReplace(updatedLink);
            ++outcome.pulledFromNative;
            continue;
        }

        // Conflict: NativeContactReconciliation::decide() deliberately stops
        // at "both sides moved" -- Contact.updatedAt vs. the native summary's
        // modifiedAt breaks the tie here, the one place that has both.
        const std::optional<QDateTime> nativeModifiedAt = modifiedAtByItemId.value(link.nativeItemId, std::nullopt);
        const QDateTime localUpdatedAt =
            localContact->updatedAt ? QDateTime::fromString(*localContact->updatedAt, Qt::ISODate) : QDateTime();

        bool nativeWins;
        if (!nativeModifiedAt.has_value()) {
            // No native timestamp at all -- native wins deterministically,
            // tracked as its own counter rather than folded into
            // pulledFromNative since this isn't a genuine timestamp
            // comparison.
            nativeWins = true;
            ++outcome.conflictsResolvedNativeWins;
        } else if (!localUpdatedAt.isValid() || *nativeModifiedAt > localUpdatedAt) {
            nativeWins = true;
            ++outcome.pulledFromNative;
        } else {
            nativeWins = false;
            ++outcome.pushedToNative;
        }

        if (nativeWins) {
            Contact updated = VCardContact::contactFromVCard(*nativeVCard);
            updated.uid = localContact->uid;
            updated.rev = localContact->rev;
            m_contactRepo.queueUpdate(updated);
        } else {
            m_provider.updateItem(link.nativeItemId, VCardContact::contactToVCard(*localContact));
        }

        NativeContactLink updatedLink = link;
        updatedLink.lastSyncedHash = nativeWins ? nativeHash : localHash;
        updatedLink.lastSyncedAt = syncedAt;
        m_linkDao.insertOrReplace(updatedLink);
    }

    return outcome;
}
