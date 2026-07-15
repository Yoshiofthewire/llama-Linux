#pragma once

#include "contacts/native/NativeContactsProvider.h"

#include <QString>

class ContactSyncRepository;
class NativeContactLinkDao;

struct NativeContactSyncOutcome
{
    NativeContactsAvailability availability = NativeContactsAvailability::Available;
    int pushedToNative = 0;
    int pulledFromNative = 0;
    int conflictsResolvedNativeWins = 0; // Conflict where the native side had no modifiedAt -- distinct
                                          // from pulledFromNative, which covers a genuine timestamp win.
    int createdOnNative = 0;
    int createdLocally = 0;
    QString detail;
};

// Orchestrates one backend's ('akonadi' | 'eds') sync pass against
// NativeContactsProvider, ContactSyncRepository (the relay-facing contact
// repository, Task 3), and NativeContactLinkDao (Task 2). A NEW class, not
// an extension of ContactSyncRepository -- anything touching
// NativeContactsProvider would otherwise pull QtDBus/Akonadi-shaped concepts
// toward core/domain, which must never depend on them. Lives in app/ for
// the same reason the real providers (Tasks 7-8) will.
//
// Precondition: any prior ContactSyncRepository::sync() call's
// uidReassignments must already have been applied via
// NativeContactLinkDao::rekeyLocalUid() by some other caller before sync()
// runs here (Task 9, out of scope this session) -- this class only reads
// whatever contactRepo.findByUid()/contacts() report at the time sync()
// runs, and does not itself guard against a temp uid having gone stale.
class NativeContactSyncRepository
{
public:
    NativeContactSyncRepository(NativeContactsProvider& provider, ContactSyncRepository& contactRepo,
                                 NativeContactLinkDao& linkDao);

    NativeContactSyncOutcome sync();

private:
    NativeContactSyncOutcome firstEnableSync(const QString& backendId);
    NativeContactSyncOutcome steadyStateSync(const QString& backendId);

    NativeContactsProvider& m_provider;
    ContactSyncRepository& m_contactRepo;
    NativeContactLinkDao& m_linkDao;
};
