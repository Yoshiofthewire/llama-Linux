#include "pgp/PgpQrTargetValidator.h"

#include <QTest>

// Covers isSafeQrTarget() in isolation, with an injected HostResolver so the
// DNS-based link-local bypass fix (VibeSec finding) is testable without
// depending on real DNS resolution.
class PgpQrTargetValidatorTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonHttpScheme();
    void rejectsEmptyHost();
    void rejectsMetadataGoogleInternalHostnameWithoutResolving();
    void rejectsLiteralLinkLocalIpViaDefaultResolver();
    void rejectsHostnameThatResolvesToLinkLocalAddress();
    void allowsHostnameThatResolvesToPublicAddress();
    void allowsHostnameThatResolvesToLoopbackAddress();
    void rejectsUnresolvableHostname();
};

void PgpQrTargetValidatorTest::rejectsNonHttpScheme()
{
    // file:// must never reach a resolver at all -- reading local files
    // back as if they were key material is the risk this blocks.
    bool resolverCalled = false;
    const HostResolver resolver = [&resolverCalled](const QString&) {
        resolverCalled = true;
        return QList<QHostAddress>{ QHostAddress(QStringLiteral("203.0.113.10")) };
    };

    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("file:///etc/passwd")), resolver));
    QVERIFY(!resolverCalled);
}

void PgpQrTargetValidatorTest::rejectsEmptyHost()
{
    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("http:///api/pgp/qr/key"))));
}

void PgpQrTargetValidatorTest::rejectsMetadataGoogleInternalHostnameWithoutResolving()
{
    bool resolverCalled = false;
    const HostResolver resolver = [&resolverCalled](const QString&) {
        resolverCalled = true;
        return QList<QHostAddress>{ QHostAddress(QStringLiteral("203.0.113.10")) };
    };

    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("http://metadata.google.internal/api/pgp/qr/key")), resolver));
    QVERIFY(!resolverCalled);
}

void PgpQrTargetValidatorTest::rejectsLiteralLinkLocalIpViaDefaultResolver()
{
    // No injected resolver -- exercises the real default resolver's literal
    // IP fast path (no DNS query needed).
    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("http://169.254.169.254/api/pgp/qr/key"))));
}

void PgpQrTargetValidatorTest::rejectsHostnameThatResolvesToLinkLocalAddress()
{
    // VibeSec regression guard: this is the actual bypass -- an ordinary
    // hostname (not a literal IP) whose DNS record points at a link-local/
    // cloud-metadata address used to sail straight through, since the old
    // check only ran QHostAddress::setAddress() on the literal QR text.
    const HostResolver resolver = [](const QString& host) -> QList<QHostAddress> {
        if (host == QStringLiteral("attacker-domain.example"))
            return { QHostAddress(QStringLiteral("169.254.169.254")) };
        return {};
    };

    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("http://attacker-domain.example/api/pgp/qr/key")), resolver));
}

void PgpQrTargetValidatorTest::allowsHostnameThatResolvesToPublicAddress()
{
    const HostResolver resolver = [](const QString& host) -> QList<QHostAddress> {
        if (host == QStringLiteral("relay.example"))
            return { QHostAddress(QStringLiteral("203.0.113.10")) };
        return {};
    };

    QVERIFY(isSafeQrTarget(QUrl(QStringLiteral("https://relay.example/api/pgp/qr/key")), resolver));
}

void PgpQrTargetValidatorTest::allowsHostnameThatResolvesToLoopbackAddress()
{
    // Self-hosted relays commonly live on localhost -- must stay allowed.
    const HostResolver resolver = [](const QString& host) -> QList<QHostAddress> {
        if (host == QStringLiteral("my-relay.localdomain"))
            return { QHostAddress(QStringLiteral("127.0.0.1")) };
        return {};
    };

    QVERIFY(isSafeQrTarget(QUrl(QStringLiteral("http://my-relay.localdomain/api/pgp/qr/key")), resolver));
}

void PgpQrTargetValidatorTest::rejectsUnresolvableHostname()
{
    // Fail closed: if the safety of a host can't be verified, don't
    // proceed.
    const HostResolver resolver = [](const QString&) { return QList<QHostAddress>{}; };

    QVERIFY(!isSafeQrTarget(QUrl(QStringLiteral("http://does-not-resolve.example/api/pgp/qr/key")), resolver));
}

QTEST_GUILESS_MAIN(PgpQrTargetValidatorTest)
#include "PgpQrTargetValidatorTest.moc"
