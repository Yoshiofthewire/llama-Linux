#include "domain/MailRepository.h"

#include "db/EmailDao.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/RelayAuth.h"
#include "net/RelayMailSource.h"
#include "stores/CursorStore.h"

#include <QMap>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace {

// Display-only bucket the backend falls back to for messages matching no
// real keyword (bucket(e.Keywords, ...) in the Go source) -- excluded from
// Email::keywords the same way KeywordTabs.kt/KeywordRepository.swift only
// ever deal in real keyword names.
const QString kUncategorizedTab = QStringLiteral("Uncategorized");

MailRepositoryOutcome outcomeFromNetworkError(NetworkError error)
{
    switch (error) {
    case NetworkError::Unauthorized:
        return MailRepositoryOutcome::Unauthorized;
    case NetworkError::ServiceUnavailable:
        return MailRepositoryOutcome::ServiceUnavailable;
    default:
        return MailRepositoryOutcome::Retry;
    }
}

} // namespace

MailRepository::MailRepository(RelayMailSource& source, EmailDao& emailDao, PairingStore& pairingStore,
                                CursorStore& cursorStore)
    : m_source(source)
    , m_emailDao(emailDao)
    , m_pairingStore(pairingStore)
    , m_cursorStore(cursorStore)
{
}

QVector<Email> MailRepository::cachedEmails(const QString& folder) const
{
    return m_emailDao.findByFolder(folder);
}

MailFetchOutcome MailRepository::refreshFolder(const QString& folder, bool forceFullResync)
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value())
        return { MailRepositoryOutcome::NotPaired, QStringLiteral("Not paired") };

    const RelayAuth auth{ pairing->subscriberId, pairing->subscriberHash };
    const QUrl serverBaseUrl(pairing->serverBaseUrl);

    std::optional<qint64> since;
    if (forceFullResync) {
        since = 0;
    } else {
        const QString storedCursor = m_cursorStore.mailCursor();
        if (!storedCursor.isEmpty())
            since = storedCursor.toLongLong();
    }

    const InboxFetchResult result = m_source.fetchInbox(serverBaseUrl, auth, std::nullopt, folder, since);
    if (result.error.has_value())
        return { outcomeFromNetworkError(*result.error), result.detail };

    // The backend buckets one message into every keyword-tab it matches, so
    // the same messageId can appear as a separate copy in more than one
    // byTab array. Group by messageId, take any one copy as the base row,
    // collect the (non-Uncategorized) tab names it appeared under into
    // Email::keywords, and stamp Email::folder with the mailbox that was
    // requested rather than the tab name RelayMailSource set it to --
    // RelayMailSource intentionally leaves this mapping to the domain layer
    // (see its own header comments).
    QMap<QString, Email> emailsById;
    QMap<QString, QSet<QString>> keywordsById;
    QStringList order;

    for (auto it = result.byTab.constBegin(); it != result.byTab.constEnd(); ++it) {
        const QString& tab = it.key();
        for (const InboxEmailItem& item : it.value()) {
            const QString& id = item.email.messageId;
            if (!emailsById.contains(id)) {
                emailsById.insert(id, item.email);
                order.append(id);
            }
            if (tab != kUncategorizedTab)
                keywordsById[id].insert(tab);
        }
    }

    QVector<Email> emails;
    emails.reserve(order.size());
    for (const QString& id : order) {
        Email email = emailsById.value(id);
        email.folder = folder;
        QStringList keywords = keywordsById.value(id).values();
        std::sort(keywords.begin(), keywords.end());
        email.keywords = keywords;
        emails.append(email);
    }

    if (!result.isDelta) {
        m_emailDao.replaceFolderSnapshot(folder, emails);
    } else {
        // insertOrReplace is already upsert-by-messageId, matching Android's
        // "new" + "updated" both going through a single upsert path -- no
        // separate body/preview-preservation merge is needed since
        // RelayMailSource doesn't special-case delta bodies differently from
        // full-snapshot bodies (there is nothing to preserve the wire didn't
        // already provide).
        for (const Email& email : emails)
            m_emailDao.insertOrReplace(email);
        for (const QString& id : result.removed)
            m_emailDao.deleteById(id);
        m_cursorStore.setMailCursor(QString::number(result.cursor));
    }

    return { MailRepositoryOutcome::Success, QString() };
}
