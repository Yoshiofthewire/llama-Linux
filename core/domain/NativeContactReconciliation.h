#pragma once

#include "models/Contact.h"

#include <QPair>
#include <QString>
#include <QVector>

struct NativeMatchResult
{
    QVector<QPair<QString /*localUid*/, QString /*nativeItemId*/>> matched;
    QVector<QString> unmatchedLocalUids;     // needs export to native
    QVector<QString> unmatchedNativeItemIds; // needs import to local
};

// First-enable matching (matchUnlinked) plus steady-state conflict
// classification (decide) for the native-contacts backend. There is no wire
// protocol here like ContactSyncReconciliation's server round-trip, just two
// snapshots -- our DB and the native address book -- that have never been
// linked before, so matching is pure content heuristics: pass 1, exact
// case-insensitive/trimmed email (the strongest signal: two independently
// authored contacts sharing an email address are almost certainly the same
// person); pass 2, normalized-phone suffix match for anything email didn't
// resolve (a weaker but still useful signal once email is exhausted, e.g.
// contacts synced with different/no email at all). Each side is used at
// most once per pass -- once matched, an entry is removed from further
// consideration -- so an email or phone shared by more than one entry can
// only ever pair with one counterpart; anything left over after both
// passes is a genuine new contact on that side, not a conflict (first-enable
// is a merge, never a conflict).
class NativeContactReconciliation
{
public:
    static NativeMatchResult matchUnlinked(const QVector<Contact>& unlinkedLocal,
                                            const QVector<QPair<QString, Contact>>& nativeContacts);

    enum class SyncAction { NoChange, PushLocalToNative, PullNativeToLocal, Conflict };

    // Contact-level last-write-wins, not Android's field-level merge --
    // deliberately simpler since native address book providers don't
    // reliably expose field-level modification timestamps. Compares each
    // side's current content hash against the hash recorded at the last
    // successful sync: if only one side moved, that side wins outright; if
    // both moved, this is a genuine conflict and this method's job stops
    // here -- it does NOT pick a winner (see the orchestrator, which has
    // Contact.updatedAt and the native item's modified-time to break the
    // tie the way this method alone cannot).
    static SyncAction decide(const QString& lastSyncedHash, const QString& currentLocalHash,
                              const QString& currentNativeHash);
};
