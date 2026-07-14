#include "push/NtfyTopicProvisioner.h"

#include "stores/SecureStoreFile.h"

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

// Covers NtfyTopicProvisioner's generation/persistence logic against a real
// SecureStoreFile (QTemporaryDir-backed, same pattern as
// SecureStoreFileTest/PairingStoreTest) rather than a mock -- SecureStore's
// contract is tiny (set/get/remove/contains) and SecureStoreFile is already
// the repo's standard test double for it.
class NtfyTopicProvisionerTest : public QObject
{
    Q_OBJECT

private slots:
    void getOrCreateGeneratesAndPersistsOnFirstCall();
    void getOrCreateReturnsSamePersistedTopicOnSubsequentCalls();
    void getOrCreateGeneratedTopicHasAtLeast128BitsOfHexEntropy();
    void rotateTopicAlwaysGeneratesADifferentValueAndPersistsIt();
};

void NtfyTopicProvisionerTest::getOrCreateGeneratesAndPersistsOnFirstCall()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    QVERIFY(!store.contains(QStringLiteral("ntfy-topic")));

    const QString topic = NtfyTopicProvisioner::getOrCreateTopic(store);

    QVERIFY(!topic.isEmpty());
    QVERIFY(store.contains(QStringLiteral("ntfy-topic")));
    QCOMPARE(store.get(QStringLiteral("ntfy-topic")).value(), topic);
}

void NtfyTopicProvisionerTest::getOrCreateReturnsSamePersistedTopicOnSubsequentCalls()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    const QString first = NtfyTopicProvisioner::getOrCreateTopic(store);
    const QString second = NtfyTopicProvisioner::getOrCreateTopic(store);
    const QString third = NtfyTopicProvisioner::getOrCreateTopic(store);

    QCOMPARE(second, first);
    QCOMPARE(third, first);
}

void NtfyTopicProvisionerTest::getOrCreateGeneratedTopicHasAtLeast128BitsOfHexEntropy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    const QString topic = NtfyTopicProvisioner::getOrCreateTopic(store);

    // 128 bits hex-encoded is 32 hex characters -- assert the exact shape
    // rather than just non-empty, so a future accidental shrink (e.g. to a
    // 64-bit topic) fails this test.
    QCOMPARE(topic.length(), 32);
    QVERIFY(topic.contains(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$"))));
}

void NtfyTopicProvisionerTest::rotateTopicAlwaysGeneratesADifferentValueAndPersistsIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    const QString original = NtfyTopicProvisioner::getOrCreateTopic(store);
    const QString rotated = NtfyTopicProvisioner::rotateTopic(store);

    QVERIFY(!rotated.isEmpty());
    QVERIFY(rotated != original);
    QCOMPARE(store.get(QStringLiteral("ntfy-topic")).value(), rotated);

    // A subsequent getOrCreateTopic() call must see the rotated value, not
    // regenerate again or somehow return the original.
    QCOMPARE(NtfyTopicProvisioner::getOrCreateTopic(store), rotated);
}

QTEST_GUILESS_MAIN(NtfyTopicProvisionerTest)
#include "NtfyTopicProvisionerTest.moc"
