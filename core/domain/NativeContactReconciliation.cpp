#include "domain/NativeContactReconciliation.h"

namespace {

QString normalizedEmail(const Contact& contact)
{
    if (contact.emails.isEmpty())
        return QString();
    return contact.emails.first().value.trimmed().toCaseFolded();
}

// Strips everything but digits, then keeps only the trailing
// kPhoneSuffixLength of them. Ten digits is the length of a NANP national
// significant number (area code + subscriber number) and, not
// coincidentally, close to the typical national-number length for most
// other countries too -- so two renderings of the same number that differ
// only in country-code prefix ("+44 20 7946 0958" vs "020 7946 0958") or
// in punctuation/spacing still reduce to the same suffix. A shorter suffix
// would risk false-positive matches between genuinely different numbers
// that happen to share a subscriber-number tail; a longer one would fail to
// absorb country-code differences, which is the whole point of this pass.
constexpr int kPhoneSuffixLength = 10;

QString normalizedPhoneSuffix(const Contact& contact)
{
    if (contact.phones.isEmpty())
        return QString();

    QString digits;
    for (const QChar& ch : contact.phones.first().value) {
        if (ch.isDigit())
            digits.append(ch);
    }
    if (digits.isEmpty())
        return QString();

    return digits.right(kPhoneSuffixLength);
}

} // namespace

NativeMatchResult NativeContactReconciliation::matchUnlinked(
    const QVector<Contact>& unlinkedLocal, const QVector<QPair<QString, Contact>>& nativeContacts)
{
    QVector<QPair<QString, Contact>> candidates = nativeContacts;

    QVector<Contact> unmatchedAfterEmail;
    NativeMatchResult result;

    // Pass 1: exact case-insensitive/trimmed email match.
    for (const Contact& local : unlinkedLocal) {
        const QString email = normalizedEmail(local);

        int matchIndex = -1;
        if (!email.isEmpty()) {
            for (int i = 0; i < candidates.size(); ++i) {
                if (normalizedEmail(candidates.at(i).second) == email) {
                    matchIndex = i;
                    break;
                }
            }
        }

        if (matchIndex >= 0) {
            result.matched.append({ local.uid, candidates.at(matchIndex).first });
            candidates.remove(matchIndex);
        } else {
            unmatchedAfterEmail.append(local);
        }
    }

    // Pass 2: normalized-phone suffix match for whatever pass 1 couldn't
    // resolve.
    for (const Contact& local : unmatchedAfterEmail) {
        const QString phoneSuffix = normalizedPhoneSuffix(local);

        int matchIndex = -1;
        if (!phoneSuffix.isEmpty()) {
            for (int i = 0; i < candidates.size(); ++i) {
                if (normalizedPhoneSuffix(candidates.at(i).second) == phoneSuffix) {
                    matchIndex = i;
                    break;
                }
            }
        }

        if (matchIndex >= 0) {
            result.matched.append({ local.uid, candidates.at(matchIndex).first });
            candidates.remove(matchIndex);
        } else {
            result.unmatchedLocalUids.append(local.uid);
        }
    }

    for (const auto& candidate : candidates)
        result.unmatchedNativeItemIds.append(candidate.first);

    return result;
}

NativeContactReconciliation::SyncAction NativeContactReconciliation::decide(const QString& lastSyncedHash,
                                                                             const QString& currentLocalHash,
                                                                             const QString& currentNativeHash)
{
    const bool localChanged = currentLocalHash != lastSyncedHash;
    const bool nativeChanged = currentNativeHash != lastSyncedHash;

    if (!localChanged && !nativeChanged)
        return SyncAction::NoChange;
    if (localChanged && !nativeChanged)
        return SyncAction::PushLocalToNative;
    if (!localChanged && nativeChanged)
        return SyncAction::PullNativeToLocal;
    return SyncAction::Conflict;
}
