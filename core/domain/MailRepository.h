#pragma once

#include "models/Email.h"

#include <QString>
#include <QVector>

class RelayMailSource;
class EmailDao;
class PairingStore;
class CursorStore;

enum class MailRepositoryOutcome { Success, NotPaired, Unauthorized, ServiceUnavailable, Retry };

struct MailFetchOutcome
{
    MailRepositoryOutcome outcome = MailRepositoryOutcome::Retry;
    QString detail; // meaningful when outcome != Success
};

// Sits between RelayMailSource and EmailDao, matching the Domain/
// Repositories layer in llama-Mail-for-Mac (its MailRepository does a dumb
// full-replace with no delta-merge; Android's MailRepository.kt/
// reconcileFetchResult is the closer reference for the delta-merge logic
// here). Also owns the two wire-mapping fixes RelayMailSource deliberately
// left to this layer (see refreshFolder()'s .cpp comments): populating
// Email::keywords from the per-response byTab buckets, and correcting
// Email::folder to the requested mailbox rather than the tab name
// RelayMailSource stamps on each item.
class MailRepository
{
public:
    MailRepository(RelayMailSource& source, EmailDao& emailDao, PairingStore& pairingStore,
                    CursorStore& cursorStore);

    QVector<Email> cachedEmails(const QString& folder) const;

    // forceFullResync sends since=0 explicitly (bypassing the stored
    // cursor) for a user-initiated manual refresh; otherwise this method
    // sends the CursorStore-persisted mail cursor (omitted when empty --
    // first-ever fetch for this folder is a full snapshot).
    MailFetchOutcome refreshFolder(const QString& folder, bool forceFullResync = false);

private:
    RelayMailSource& m_source;
    EmailDao& m_emailDao;
    PairingStore& m_pairingStore;
    CursorStore& m_cursorStore;
};
