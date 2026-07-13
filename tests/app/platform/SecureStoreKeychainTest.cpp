#include "platform/SecureStoreKeychain.h"

#include <qt6keychain/keychain.h>

#include <QTest>
#include <QUuid>

// Exercises SecureStoreKeychain against whatever Secret Service provider is
// reachable on this machine (gnome-keyring / ksecretd / kwallet all
// implement org.freedesktop.secrets). The service name is unique per test
// run so a stray failure can't collide with a real credential, and the test
// removes its own entry in cleanup() regardless of outcome.
class SecureStoreKeychainTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void roundTripsSetGetContainsRemove();

private:
    QString m_service;
};

void SecureStoreKeychainTest::init()
{
    m_service = QStringLiteral("llama-mail-securestore-test-%1")
                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void SecureStoreKeychainTest::cleanup()
{
    SecureStoreKeychain store(m_service);
    store.remove(QStringLiteral("sub"));
}

void SecureStoreKeychainTest::roundTripsSetGetContainsRemove()
{
    SecureStoreKeychain store(m_service);

    const bool wrote = store.set(QStringLiteral("sub"), QStringLiteral("subscriber-123"));
    if (!wrote) {
        QKeychain::ReadPasswordJob probe(m_service);
        probe.setAutoDelete(false);
        probe.setKey(QStringLiteral("sub"));
        QEventLoop loop;
        connect(&probe, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
        probe.start();
        loop.exec();
        QSKIP(qPrintable(QStringLiteral(
                  "No usable Secret Service backend reachable (write failed, error=%1: %2)")
                  .arg(int(probe.error()))
                  .arg(probe.errorString())));
    }

    QVERIFY(store.contains(QStringLiteral("sub")));
    QCOMPARE(store.get(QStringLiteral("sub")).value(), QStringLiteral("subscriber-123"));

    QVERIFY(store.remove(QStringLiteral("sub")));
    QVERIFY(!store.contains(QStringLiteral("sub")));
    QVERIFY(!store.get(QStringLiteral("sub")).has_value());
}

QTEST_GUILESS_MAIN(SecureStoreKeychainTest)
#include "SecureStoreKeychainTest.moc"
