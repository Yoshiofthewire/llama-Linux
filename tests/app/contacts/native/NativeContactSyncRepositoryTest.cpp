#include "contacts/native/NativeContactSyncRepository.h"

#include "contacts/native/NativeContactsProvider.h"
#include "db/ContactDao.h"
#include "db/Database.h"
#include "db/NativeContactLinkDao.h"
#include "db/PendingContactChangeDao.h"
#include "domain/ContactSyncRepository.h"
#include "domain/PairingStore.h"
#include "net/ContactSyncClient.h"
#include "net/HttpClient.h"
#include "stores/CursorStore.h"
#include "stores/SecureStoreFile.h"
#include "vcard/VCardContact.h"

#include "FakeNativeContactsProvider.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Everything NativeContactSyncRepository needs wired up, plus the fake
// provider it talks to -- bundled so each test slot doesn't repeat ~15
// lines of DB/store plumbing. Members are declared in dependency order
// (each one only needs objects declared above it); db.open() happens in the
// constructor body, after every reference member below has already bound to
// db's internal QSqlDatabase, which remains the same object (just not yet
// open) at bind time -- same pattern the rest of this test suite uses, just
// inverted in order since this is an aggregate rather than a flat function.
struct Env
{
    Database db;
    ContactDao contactDao;
    PendingContactChangeDao pendingDao;
    QTemporaryDir cursorDir;
    CursorStore cursorStore;
    QTemporaryDir secureDir;
    SecureStoreFile secureStore;
    PairingStore pairingStore;
    QNetworkAccessManager manager;
    HttpClient http;
    ContactSyncClient client;
    ContactSyncRepository contactRepo;
    NativeContactLinkDao linkDao;
    FakeNativeContactsProvider provider;
    NativeContactSyncRepository nativeRepo;

    Env()
        : contactDao(db.handle())
        , pendingDao(db.handle())
        , cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")))
        , secureStore(secureDir.path())
        , pairingStore(secureStore)
        , http(manager)
        , client(http)
        , contactRepo(client, contactDao, pendingDao, cursorStore, pairingStore)
        , linkDao(db.handle())
        , nativeRepo(provider, contactRepo, linkDao)
    {
        db.open(QStringLiteral(":memory:"));
    }
};

Contact makeContact(const QString& uid, const QString& fn, const QString& email = QString())
{
    Contact c;
    c.uid = uid;
    c.fn = fn;
    if (!email.isEmpty())
        c.emails = { ContactEmailEntry{ std::nullopt, email } };
    return c;
}

// Seeds a local contact and a matching (same email, so matchUnlinked pairs
// them) native item with identical vCard content, then runs the
// first-enable sync() pass that links them -- the common baseline every
// steady-state test below builds on, so those tests never need to know how
// NativeContactSyncRepository computes its internal hash.
QString establishLinkedPair(Env& env, const QString& localUid, const QString& fn, const QString& email)
{
    Contact local = makeContact(localUid, fn, email);
    if (!env.contactDao.insertOrReplace(local))
        return QString();

    Contact native = local;
    native.uid = QString(); // native side has no relay-uid concept
    const QString itemId = env.provider.seedItem(VCardContact::contactToVCard(native));

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();
    if (outcome.availability != NativeContactsAvailability::Available)
        return QString();

    return itemId;
}

} // namespace

class NativeContactSyncRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void firstEnableMatchesLinksAndImportsBothSides();
    void steadyStateNoChangeLeavesEverythingAsIs();
    void steadyStatePushSendsLocalEditToNative();
    void steadyStatePullAppliesNativeEditLocally();
    void conflictWithBothTimestampsLaterWins();
    void conflictWithNoNativeTimestampNativeWinsDeterministically();
    void nativeSideDeletionAppliesLocalDelete();
    void localSideDeletionAppliesNativeDelete();
    void unavailableProviderShortCircuitsWithNoSideEffects();
};

void NativeContactSyncRepositoryTest::firstEnableMatchesLinksAndImportsBothSides()
{
    Env env;

    // Ada exists locally and matches (by email) a native item -- should be
    // linked with no push/pull/create, no duplicate.
    Contact ada = makeContact(QStringLiteral("local-ada"), QStringLiteral("Ada Lovelace"),
                               QStringLiteral("ada@example.com"));
    QVERIFY(env.contactDao.insertOrReplace(ada));

    // Bob exists locally only -- should be exported to the native side.
    Contact bob = makeContact(QStringLiteral("local-bob"), QStringLiteral("Bob Builder"));
    QVERIFY(env.contactDao.insertOrReplace(bob));

    Contact adaNative = ada;
    adaNative.uid = QString();
    const QString adaNativeItemId = env.provider.seedItem(VCardContact::contactToVCard(adaNative));

    // Grace exists on the native side only -- should be imported locally.
    Contact grace = makeContact(QString(), QStringLiteral("Grace Hopper"), QStringLiteral("grace@example.com"));
    const QString graceNativeItemId = env.provider.seedItem(VCardContact::contactToVCard(grace));

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.availability, NativeContactsAvailability::Available);
    QCOMPARE(outcome.createdLocally, 1);
    QCOMPARE(outcome.createdOnNative, 1);
    QCOMPARE(outcome.pushedToNative, 0);
    QCOMPARE(outcome.pulledFromNative, 0);
    QCOMPARE(outcome.conflictsResolvedNativeWins, 0);

    QCOMPARE(env.linkDao.findAllForBackend(QStringLiteral("fake")).size(), 3);

    const auto adaLink = env.linkDao.findByLocalUid(QStringLiteral("local-ada"), QStringLiteral("fake"));
    QVERIFY(adaLink.has_value());
    QCOMPARE(adaLink->nativeItemId, adaNativeItemId);

    const auto bobLink = env.linkDao.findByLocalUid(QStringLiteral("local-bob"), QStringLiteral("fake"));
    QVERIFY(bobLink.has_value());
    QVERIFY(env.provider.hasItem(bobLink->nativeItemId));
    QVERIFY(env.provider.vCardFor(bobLink->nativeItemId).contains(QStringLiteral("Bob Builder")));

    const auto graceLink = env.linkDao.findByNativeItemId(QStringLiteral("fake"), graceNativeItemId);
    QVERIFY(graceLink.has_value());
    const auto graceLocal = env.contactRepo.findByUid(graceLink->localUid);
    QVERIFY(graceLocal.has_value());
    QCOMPARE(*graceLocal->fn, QStringLiteral("Grace Hopper"));

    // No duplicates: exactly 3 local contacts (Ada, Bob, Grace) and 3 native
    // items (Ada, Grace's original seed, Bob's new export).
    QCOMPARE(env.contactRepo.contacts().size(), 3);
    QCOMPARE(env.provider.itemCount(), 3);
}

void NativeContactSyncRepositoryTest::steadyStateNoChangeLeavesEverythingAsIs()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());

    const QString vCardBefore = env.provider.vCardFor(itemId);
    const Contact contactBefore = *env.contactRepo.findByUid(QStringLiteral("local-1"));

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.pushedToNative, 0);
    QCOMPARE(outcome.pulledFromNative, 0);
    QCOMPARE(outcome.conflictsResolvedNativeWins, 0);
    QCOMPARE(outcome.createdLocally, 0);
    QCOMPARE(outcome.createdOnNative, 0);

    QCOMPARE(env.provider.vCardFor(itemId), vCardBefore);
    QCOMPARE(*env.contactRepo.findByUid(QStringLiteral("local-1")), contactBefore);
}

void NativeContactSyncRepositoryTest::steadyStatePushSendsLocalEditToNative()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());

    Contact edited = *env.contactRepo.findByUid(QStringLiteral("local-1"));
    edited.fn = QStringLiteral("Ada Byron");
    QVERIFY(env.contactDao.insertOrReplace(edited)); // local-only edit, bypassing queueUpdate (no relay push needed here)

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.pushedToNative, 1);
    QCOMPARE(outcome.pulledFromNative, 0);
    QCOMPARE(outcome.conflictsResolvedNativeWins, 0);
    QVERIFY(env.provider.vCardFor(itemId).contains(QStringLiteral("Ada Byron")));

    // A second sync() with nothing further changed must be a no-op -- proves
    // the link's hash was actually updated, not left stale.
    const NativeContactSyncOutcome second = env.nativeRepo.sync();
    QCOMPARE(second.pushedToNative, 0);
    QCOMPARE(second.pulledFromNative, 0);
}

void NativeContactSyncRepositoryTest::steadyStatePullAppliesNativeEditLocally()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());

    Contact editedNative = makeContact(QString(), QStringLiteral("Ada Byron"), QStringLiteral("ada@example.com"));
    QVERIFY(env.provider.updateItem(itemId, VCardContact::contactToVCard(editedNative)));

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.pulledFromNative, 1);
    QCOMPARE(outcome.pushedToNative, 0);
    QCOMPARE(outcome.conflictsResolvedNativeWins, 0);

    const auto updated = env.contactRepo.findByUid(QStringLiteral("local-1"));
    QVERIFY(updated.has_value());
    QCOMPARE(*updated->fn, QStringLiteral("Ada Byron"));
    QCOMPARE(updated->uid, QStringLiteral("local-1")); // relay uid preserved, never overwritten by the pull

    const NativeContactSyncOutcome second = env.nativeRepo.sync();
    QCOMPARE(second.pushedToNative, 0);
    QCOMPARE(second.pulledFromNative, 0);
}

void NativeContactSyncRepositoryTest::conflictWithBothTimestampsLaterWins()
{
    // Sub-case A: the native side's modifiedAt is later -- native wins, the
    // local contact takes the native content.
    {
        Env env;
        const QString itemId = establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"),
                                                     QStringLiteral("ada@example.com"));
        QVERIFY(!itemId.isEmpty());

        Contact editedLocal = *env.contactRepo.findByUid(QStringLiteral("local-1"));
        editedLocal.fn = QStringLiteral("Local Edit");
        editedLocal.updatedAt = QStringLiteral("2026-01-01T00:00:00Z");
        QVERIFY(env.contactDao.insertOrReplace(editedLocal));

        Contact editedNative = makeContact(QString(), QStringLiteral("Native Edit"), QStringLiteral("ada@example.com"));
        QVERIFY(env.provider.updateItem(itemId, VCardContact::contactToVCard(editedNative)));
        env.provider.setModifiedAt(itemId, QDateTime::fromString(QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate));

        const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

        QCOMPARE(outcome.pulledFromNative, 1);
        QCOMPARE(outcome.pushedToNative, 0);
        QCOMPARE(outcome.conflictsResolvedNativeWins, 0);
        QCOMPARE(*env.contactRepo.findByUid(QStringLiteral("local-1"))->fn, QStringLiteral("Native Edit"));
    }

    // Sub-case B: the local contact's updatedAt is later -- local wins, the
    // native item takes the local content.
    {
        Env env;
        const QString itemId = establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"),
                                                     QStringLiteral("ada@example.com"));
        QVERIFY(!itemId.isEmpty());

        Contact editedLocal = *env.contactRepo.findByUid(QStringLiteral("local-1"));
        editedLocal.fn = QStringLiteral("Local Edit");
        editedLocal.updatedAt = QStringLiteral("2026-01-05T00:00:00Z");
        QVERIFY(env.contactDao.insertOrReplace(editedLocal));

        Contact editedNative = makeContact(QString(), QStringLiteral("Native Edit"), QStringLiteral("ada@example.com"));
        QVERIFY(env.provider.updateItem(itemId, VCardContact::contactToVCard(editedNative)));
        env.provider.setModifiedAt(itemId, QDateTime::fromString(QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate));

        const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

        QCOMPARE(outcome.pushedToNative, 1);
        QCOMPARE(outcome.pulledFromNative, 0);
        QCOMPARE(outcome.conflictsResolvedNativeWins, 0);
        QVERIFY(env.provider.vCardFor(itemId).contains(QStringLiteral("Local Edit")));
    }
}

void NativeContactSyncRepositoryTest::conflictWithNoNativeTimestampNativeWinsDeterministically()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());

    Contact editedLocal = *env.contactRepo.findByUid(QStringLiteral("local-1"));
    editedLocal.fn = QStringLiteral("Local Edit");
    // Deliberately very recent -- proves recency alone doesn't win when the
    // native side has no modifiedAt at all, per the deterministic rule.
    editedLocal.updatedAt = QStringLiteral("2026-07-15T00:00:00Z");
    QVERIFY(env.contactDao.insertOrReplace(editedLocal));

    Contact editedNative = makeContact(QString(), QStringLiteral("Native Edit"), QStringLiteral("ada@example.com"));
    QVERIFY(env.provider.updateItem(itemId, VCardContact::contactToVCard(editedNative)));
    // No setModifiedAt() call -- stays nullopt, the case under test.

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.conflictsResolvedNativeWins, 1);
    QCOMPARE(outcome.pulledFromNative, 0);
    QCOMPARE(outcome.pushedToNative, 0);
    QCOMPARE(*env.contactRepo.findByUid(QStringLiteral("local-1"))->fn, QStringLiteral("Native Edit"));
}

void NativeContactSyncRepositoryTest::nativeSideDeletionAppliesLocalDelete()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());
    QVERIFY(env.pendingDao.findAll().isEmpty());

    // Simulate the item vanishing on the native side between syncs (deleted
    // in KAddressBook/GNOME Contacts directly, outside this app).
    QVERIFY(env.provider.deleteItem(itemId));

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QVERIFY(!env.contactRepo.findByUid(QStringLiteral("local-1")).has_value());
    QCOMPARE(env.pendingDao.findAll().size(), 1); // queueDelete's tombstone was enqueued
    QVERIFY(!env.linkDao.findByLocalUid(QStringLiteral("local-1"), QStringLiteral("fake")).has_value());
    Q_UNUSED(outcome); // no dedicated counter for deletions in NativeContactSyncOutcome per the brief
}

void NativeContactSyncRepositoryTest::localSideDeletionAppliesNativeDelete()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());
    QVERIFY(env.provider.hasItem(itemId));

    // Simulate the contact vanishing locally between syncs via some other
    // path (e.g. a relay-driven ContactSyncRepository::sync() deletion) --
    // deliberately bypassing queueDelete() so no tombstone/pending row is
    // involved, just the row itself gone from the local cache.
    QVERIFY(env.contactDao.deleteById(QStringLiteral("local-1")));

    env.nativeRepo.sync();

    QVERIFY(!env.provider.hasItem(itemId));
    QVERIFY(!env.linkDao.findByLocalUid(QStringLiteral("local-1"), QStringLiteral("fake")).has_value());
}

void NativeContactSyncRepositoryTest::unavailableProviderShortCircuitsWithNoSideEffects()
{
    Env env;
    const QString itemId =
        establishLinkedPair(env, QStringLiteral("local-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com"));
    QVERIFY(!itemId.isEmpty());

    // An extra unmatched contact/native item that a first-enable-shaped pass
    // would otherwise create/import -- must stay untouched too.
    Contact extraLocal = makeContact(QStringLiteral("local-2"), QStringLiteral("Untouched Local"));
    QVERIFY(env.contactDao.insertOrReplace(extraLocal));
    Contact extraNative = makeContact(QString(), QStringLiteral("Untouched Native"));
    const QString extraNativeItemId = env.provider.seedItem(VCardContact::contactToVCard(extraNative));

    const int contactsBefore = env.contactRepo.contacts().size();
    const int itemsBefore = env.provider.itemCount();
    const int linksBefore = env.linkDao.findAllForBackend(QStringLiteral("fake")).size();

    env.provider.setAvailability(NativeContactsAvailability::ServiceNotRunning);

    const NativeContactSyncOutcome outcome = env.nativeRepo.sync();

    QCOMPARE(outcome.availability, NativeContactsAvailability::ServiceNotRunning);
    QCOMPARE(outcome.pushedToNative, 0);
    QCOMPARE(outcome.pulledFromNative, 0);
    QCOMPARE(outcome.conflictsResolvedNativeWins, 0);
    QCOMPARE(outcome.createdOnNative, 0);
    QCOMPARE(outcome.createdLocally, 0);

    QCOMPARE(env.contactRepo.contacts().size(), contactsBefore);
    QCOMPARE(env.provider.itemCount(), itemsBefore);
    QCOMPARE(env.linkDao.findAllForBackend(QStringLiteral("fake")).size(), linksBefore);
    QVERIFY(!env.linkDao.findByLocalUid(QStringLiteral("local-2"), QStringLiteral("fake")).has_value());
    QVERIFY(!env.linkDao.findByNativeItemId(QStringLiteral("fake"), extraNativeItemId).has_value());
}

QTEST_GUILESS_MAIN(NativeContactSyncRepositoryTest)
#include "NativeContactSyncRepositoryTest.moc"
