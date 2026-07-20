#include "stores/SecureStoreFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

class SecureStoreFileTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsSetGetContainsRemove();
    void getMissingKeyReturnsNullopt();
    void createdFileIsOwnerReadWriteOnly();
    void rejectsKeysThatEscapeTheStoreDirectory();
    void setAppliesRestrictivePermissionsBeforeWritingContent();
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

// A key must be an opaque identifier for a file directly inside the store's
// directory. QDir::filePath() does not sanitize ".." components and returns
// an absolute second argument verbatim, so without validation a malicious or
// buggy caller-supplied key could read/write/delete arbitrary files. Every
// operation must reject these keys outright rather than sanitize them.
void SecureStoreFileTest::rejectsKeysThatEscapeTheStoreDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile store(dir.path());

    QTemporaryDir outsideDir;
    QVERIFY(outsideDir.isValid());
    const QString absoluteEscapeTarget = outsideDir.filePath(QStringLiteral("absolute-escape"));

    const QStringList maliciousKeys = {
        QStringLiteral("../escaped-relative"),
        QStringLiteral("../../../../etc/kypost-should-not-be-written"),
        QStringLiteral("subdir/escaped"),
        QStringLiteral("subdir\\escaped"),
        QStringLiteral("."),
        QStringLiteral(".."),
        QStringLiteral(""),
        absoluteEscapeTarget,
    };

    for (const QString& key : maliciousKeys) {
        QVERIFY2(!store.set(key, QStringLiteral("secret")), qPrintable(key));
        QVERIFY2(!store.get(key).has_value(), qPrintable(key));
        QVERIFY2(!store.contains(key), qPrintable(key));
        QVERIFY2(!store.remove(key), qPrintable(key));
    }

    // Nothing should have been created outside the store directory.
    QVERIFY(!QFileInfo::exists(absoluteEscapeTarget));
    QDir parentOfStore(dir.path());
    QVERIFY(parentOfStore.cdUp());
    QVERIFY(!QFileInfo::exists(parentOfStore.filePath(QStringLiteral("escaped-relative"))));

    // And a well-formed key must still work normally.
    QVERIFY(store.set(QStringLiteral("deviceId"), QStringLiteral("device-1")));
    QCOMPARE(store.get(QStringLiteral("deviceId")).value(), QStringLiteral("device-1"));
}

// Regression test for a TOCTOU exposure window: the old implementation wrote
// the plaintext secret to the file and only *afterwards* called
// setPermissions(), so under a loose umask the file briefly existed at
// (e.g.) 0644 with the secret already on disk. The final-state check in
// createdFileIsOwnerReadWriteOnly() above cannot catch this because
// setPermissions() is always called eventually, so the file always ends up
// at 0600 by the time set() returns — regardless of the bug.
//
// This test uses inotify to record the kernel's actual, chronologically
// ordered event stream for the target file (IN_ATTRIB for the permission
// change, IN_MODIFY for the content write) instead of racing a poller
// against the operation. Event ordering from a single inotify queue is not
// timing-sensitive, so this deterministically proves whether the
// permission tightening happened strictly before any content was written.
void SecureStoreFileTest::setAppliesRestrictivePermissionsBeforeWritingContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const mode_t previousUmask = ::umask(0); // loosest possible, so any lapse is visible

    const int inotifyFd = inotify_init1(IN_NONBLOCK);
    QVERIFY(inotifyFd >= 0);
    const int watch = inotify_add_watch(
        inotifyFd, qPrintable(dir.path()), IN_CREATE | IN_ATTRIB | IN_MODIFY);
    QVERIFY(watch >= 0);

    SecureStoreFile store(dir.path());
    const QString key = QStringLiteral("ntfyBearerToken");
    const bool setOk = store.set(key, QStringLiteral("s3cr3t-token-value"));

    QStringList eventOrder;
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (int attempt = 0; attempt < 100 && eventOrder.count(QStringLiteral("MODIFY")) < 1;
        ++attempt) {
        const ssize_t len = ::read(inotifyFd, buffer, sizeof(buffer));
        if (len <= 0) {
            QTest::qWait(5);
            continue;
        }
        ssize_t offset = 0;
        while (offset < len) {
            const auto* event = reinterpret_cast<const struct inotify_event*>(buffer + offset);
            if (event->len > 0 && QString::fromUtf8(event->name) == key) {
                if (event->mask & IN_ATTRIB)
                    eventOrder << QStringLiteral("ATTRIB");
                if (event->mask & IN_MODIFY)
                    eventOrder << QStringLiteral("MODIFY");
            }
            offset += static_cast<ssize_t>(sizeof(struct inotify_event)) + event->len;
        }
    }
    ::close(inotifyFd);
    ::umask(previousUmask);

    QVERIFY(setOk);
    QVERIFY2(eventOrder.contains(QStringLiteral("ATTRIB")),
        "expected a permission-change (chmod) event on the key file");
    QVERIFY2(eventOrder.contains(QStringLiteral("MODIFY")),
        "expected a content-write event on the key file");
    QVERIFY2(eventOrder.indexOf(QStringLiteral("ATTRIB"))
            < eventOrder.indexOf(QStringLiteral("MODIFY")),
        qPrintable(QStringLiteral("permissions were tightened after content was written: %1")
                       .arg(eventOrder.join(QStringLiteral(",")))));
}

QTEST_GUILESS_MAIN(SecureStoreFileTest)
#include "SecureStoreFileTest.moc"
