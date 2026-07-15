#include "domain/NativeContactReconciliation.h"

#include <QTest>

class NativeContactReconciliationTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesByExactCaseInsensitiveTrimmedEmail();
    void fallsBackToNormalizedPhoneForEmailLeftovers();
    void toleratesCountryCodePrefixDifferencesInPhoneMatch();
    void eachSideUsedAtMostOnceWhenEmailIsAmbiguous();
    void leavesBothSidesUnmatchedWhenNothingIsShared();

    void decideReturnsNoChangeWhenNothingDiffersFromLastSynced();
    void decideReturnsPushLocalToNativeWhenOnlyLocalDiffers();
    void decideReturnsPullNativeToLocalWhenOnlyNativeDiffers();
    void decideReturnsConflictWhenBothDiffer();
    void decideReturnsConflictEvenWhenLocalAndNativeHashesHappenToMatchEachOther();

private:
    static Contact makeContact(const QString& uid, const QString& email = QString(),
                                const QString& phone = QString());
};

Contact NativeContactReconciliationTest::makeContact(const QString& uid, const QString& email, const QString& phone)
{
    Contact contact;
    contact.uid = uid;
    if (!email.isEmpty())
        contact.emails = { ContactEmailEntry{ std::nullopt, email } };
    if (!phone.isEmpty())
        contact.phones = { ContactPhoneEntry{ std::nullopt, phone } };
    return contact;
}

void NativeContactReconciliationTest::matchesByExactCaseInsensitiveTrimmedEmail()
{
    const QVector<Contact> local = {
        makeContact(QStringLiteral("local-ada"), QStringLiteral("  Ada@Example.com ")),
    };
    const QVector<QPair<QString, Contact>> native = {
        { QStringLiteral("native-ada"), makeContact(QString(), QStringLiteral("ada@example.com")) },
    };

    const NativeMatchResult result = NativeContactReconciliation::matchUnlinked(local, native);

    QCOMPARE(result.matched.size(), 1);
    QCOMPARE(result.matched.first().first, QStringLiteral("local-ada"));
    QCOMPARE(result.matched.first().second, QStringLiteral("native-ada"));
    QVERIFY(result.unmatchedLocalUids.isEmpty());
    QVERIFY(result.unmatchedNativeItemIds.isEmpty());
}

void NativeContactReconciliationTest::fallsBackToNormalizedPhoneForEmailLeftovers()
{
    // Emails don't match (different addresses entirely), so pass 1 leaves
    // both sides unmatched; pass 2 pairs them via normalized phone digits,
    // ignoring the punctuation/spacing differences.
    const QVector<Contact> local = {
        makeContact(QStringLiteral("local-bob"), QStringLiteral("bob@work.example"),
                    QStringLiteral("+1 (555) 123-4567")),
    };
    const QVector<QPair<QString, Contact>> native = {
        { QStringLiteral("native-bob"),
          makeContact(QString(), QStringLiteral("robert@personal.example"), QStringLiteral("555.123.4567")) },
    };

    const NativeMatchResult result = NativeContactReconciliation::matchUnlinked(local, native);

    QCOMPARE(result.matched.size(), 1);
    QCOMPARE(result.matched.first().first, QStringLiteral("local-bob"));
    QCOMPARE(result.matched.first().second, QStringLiteral("native-bob"));
    QVERIFY(result.unmatchedLocalUids.isEmpty());
    QVERIFY(result.unmatchedNativeItemIds.isEmpty());
}

void NativeContactReconciliationTest::toleratesCountryCodePrefixDifferencesInPhoneMatch()
{
    // Same UK number, one side written with the +44 country code and a
    // dropped trunk "0", the other written in purely local/national form --
    // comparing a fixed-length trailing-digit suffix absorbs this without
    // needing real libphonenumber-grade parsing.
    const QVector<Contact> local = {
        makeContact(QStringLiteral("local-uk"), QString(), QStringLiteral("+44 20 7946 0958")),
    };
    const QVector<QPair<QString, Contact>> native = {
        { QStringLiteral("native-uk"), makeContact(QString(), QString(), QStringLiteral("020 7946 0958")) },
    };

    const NativeMatchResult result = NativeContactReconciliation::matchUnlinked(local, native);

    QCOMPARE(result.matched.size(), 1);
    QCOMPARE(result.matched.first().first, QStringLiteral("local-uk"));
    QCOMPARE(result.matched.first().second, QStringLiteral("native-uk"));
}

void NativeContactReconciliationTest::eachSideUsedAtMostOnceWhenEmailIsAmbiguous()
{
    // 3 local + 3 native. "shared@example.com" is carried by two native
    // contacts; local-1 must claim only the first one it encounters, and the
    // other native entry must not be silently double-matched -- it has to
    // fall out into unmatchedNativeItemIds.
    const QVector<Contact> local = {
        makeContact(QStringLiteral("local-1"), QStringLiteral("shared@example.com")),
        makeContact(QStringLiteral("local-2"), QStringLiteral("unique2@example.com")),
        makeContact(QStringLiteral("local-3"), QString(), QStringLiteral("+1 555 000 1111")),
    };
    const QVector<QPair<QString, Contact>> native = {
        { QStringLiteral("native-1"), makeContact(QString(), QStringLiteral("shared@example.com")) },
        { QStringLiteral("native-2"), makeContact(QString(), QStringLiteral("SHARED@example.com")) },
        { QStringLiteral("native-3"), makeContact(QString(), QStringLiteral("unique2@example.com")) },
    };

    const NativeMatchResult result = NativeContactReconciliation::matchUnlinked(local, native);

    QCOMPARE(result.matched.size(), 2);
    QVERIFY(result.matched.contains({ QStringLiteral("local-1"), QStringLiteral("native-1") }));
    QVERIFY(result.matched.contains({ QStringLiteral("local-2"), QStringLiteral("native-3") }));

    QCOMPARE(result.unmatchedLocalUids.size(), 1);
    QCOMPARE(result.unmatchedLocalUids.first(), QStringLiteral("local-3"));

    QCOMPARE(result.unmatchedNativeItemIds.size(), 1);
    QCOMPARE(result.unmatchedNativeItemIds.first(), QStringLiteral("native-2"));
}

void NativeContactReconciliationTest::leavesBothSidesUnmatchedWhenNothingIsShared()
{
    const QVector<Contact> local = {
        makeContact(QStringLiteral("local-x"), QStringLiteral("x@example.com"), QStringLiteral("+1 555 111 2222")),
    };
    const QVector<QPair<QString, Contact>> native = {
        { QStringLiteral("native-y"),
          makeContact(QString(), QStringLiteral("y@example.com"), QStringLiteral("+1 555 333 4444")) },
    };

    const NativeMatchResult result = NativeContactReconciliation::matchUnlinked(local, native);

    QVERIFY(result.matched.isEmpty());
    QCOMPARE(result.unmatchedLocalUids.size(), 1);
    QCOMPARE(result.unmatchedLocalUids.first(), QStringLiteral("local-x"));
    QCOMPARE(result.unmatchedNativeItemIds.size(), 1);
    QCOMPARE(result.unmatchedNativeItemIds.first(), QStringLiteral("native-y"));
}

void NativeContactReconciliationTest::decideReturnsNoChangeWhenNothingDiffersFromLastSynced()
{
    const auto action = NativeContactReconciliation::decide(QStringLiteral("h1"), QStringLiteral("h1"),
                                                             QStringLiteral("h1"));
    QCOMPARE(action, NativeContactReconciliation::SyncAction::NoChange);
}

void NativeContactReconciliationTest::decideReturnsPushLocalToNativeWhenOnlyLocalDiffers()
{
    const auto action = NativeContactReconciliation::decide(QStringLiteral("h1"), QStringLiteral("h2"),
                                                             QStringLiteral("h1"));
    QCOMPARE(action, NativeContactReconciliation::SyncAction::PushLocalToNative);
}

void NativeContactReconciliationTest::decideReturnsPullNativeToLocalWhenOnlyNativeDiffers()
{
    const auto action = NativeContactReconciliation::decide(QStringLiteral("h1"), QStringLiteral("h1"),
                                                             QStringLiteral("h2"));
    QCOMPARE(action, NativeContactReconciliation::SyncAction::PullNativeToLocal);
}

void NativeContactReconciliationTest::decideReturnsConflictWhenBothDiffer()
{
    const auto action = NativeContactReconciliation::decide(QStringLiteral("h1"), QStringLiteral("h2"),
                                                             QStringLiteral("h3"));
    QCOMPARE(action, NativeContactReconciliation::SyncAction::Conflict);
}

void NativeContactReconciliationTest::decideReturnsConflictEvenWhenLocalAndNativeHashesHappenToMatchEachOther()
{
    // Both sides changed to the identical new content (e.g. the same edit
    // was applied twice); decide() only compares each side against
    // lastSyncedHash, so this is still Conflict -- it's the orchestrator's
    // job (not decide()'s) to notice local==native and short-circuit.
    const auto action = NativeContactReconciliation::decide(QStringLiteral("h1"), QStringLiteral("h2"),
                                                             QStringLiteral("h2"));
    QCOMPARE(action, NativeContactReconciliation::SyncAction::Conflict);
}

QTEST_APPLESS_MAIN(NativeContactReconciliationTest)
#include "NativeContactReconciliationTest.moc"
