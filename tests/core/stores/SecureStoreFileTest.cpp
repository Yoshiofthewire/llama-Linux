#include "stores/SecureStoreFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class SecureStoreFileTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsSetGetContainsRemove();
    void getMissingKeyReturnsNullopt();
    void createdFileIsOwnerReadWriteOnly();
};

void SecureStoreFileTest::roundTripsSetGetContainsRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    QVERIFY(!store.contains(QStringLiteral("sub")));

    QVERIFY(store.set(QStringLiteral("sub"), QStringLiteral("subscriber-123")));
    QVERIFY(store.contains(QStringLiteral("sub")));
    QCOMPARE(store.get(QStringLiteral("sub")).value(), QStringLiteral("subscriber-123"));

    QVERIFY(store.set(QStringLiteral("sub"), QStringLiteral("subscriber-456")));
    QCOMPARE(store.get(QStringLiteral("sub")).value(), QStringLiteral("subscriber-456"));

    QVERIFY(store.remove(QStringLiteral("sub")));
    QVERIFY(!store.contains(QStringLiteral("sub")));
    QVERIFY(!store.get(QStringLiteral("sub")).has_value());
}

void SecureStoreFileTest::getMissingKeyReturnsNullopt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    QVERIFY(!store.get(QStringLiteral("deviceId")).has_value());
}

void SecureStoreFileTest::createdFileIsOwnerReadWriteOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    QVERIFY(store.set(QStringLiteral("hash"), QStringLiteral("deadbeef")));

    QFile file(dir.filePath(QStringLiteral("hash")));
    const QFileDevice::Permissions perms = file.permissions();
    // On Unix, Qt mirrors owner permissions into both the *Owner and *User
    // flags for the same underlying bits, so both must be included in the
    // "expected" set rather than treated as separate group/other bits.
    const QFileDevice::Permissions ownerReadWrite = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser;
    const QFileDevice::Permissions everythingElse = QFileDevice::ExeOwner
        | QFileDevice::ExeUser | QFileDevice::ReadGroup | QFileDevice::WriteGroup
        | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther
        | QFileDevice::ExeOther;

    QCOMPARE(perms & ownerReadWrite, ownerReadWrite);
    QCOMPARE(perms & everythingElse, QFileDevice::Permissions());
}

QTEST_GUILESS_MAIN(SecureStoreFileTest)
#include "SecureStoreFileTest.moc"
