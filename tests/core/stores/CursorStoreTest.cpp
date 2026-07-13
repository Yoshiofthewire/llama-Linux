#include "stores/CursorStore.h"

#include <QTemporaryDir>
#include <QTest>

class CursorStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreEmpty();
    void mailCursorRoundTrips();
    void contactBaseCursorRoundTrips();
    void resetClearsBothCursors();

private:
    QString tempFilePath(QTemporaryDir& dir, const QString& name) const;
};

QString CursorStoreTest::tempFilePath(QTemporaryDir& dir, const QString& name) const
{
    return dir.filePath(name);
}

void CursorStoreTest::defaultsAreEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CursorStore store(tempFilePath(dir, QStringLiteral("cursors.ini")));

    QVERIFY(store.mailCursor().isEmpty());
    QVERIFY(store.contactBaseCursor().isEmpty());
}

void CursorStoreTest::mailCursorRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CursorStore store(tempFilePath(dir, QStringLiteral("cursors.ini")));

    store.setMailCursor(QStringLiteral("1731000000123"));
    QCOMPARE(store.mailCursor(), QStringLiteral("1731000000123"));
}

void CursorStoreTest::contactBaseCursorRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CursorStore store(tempFilePath(dir, QStringLiteral("cursors.ini")));

    store.setContactBaseCursor(QStringLiteral("rev-42")); // bare-string cursor form
    QCOMPARE(store.contactBaseCursor(), QStringLiteral("rev-42"));
}

void CursorStoreTest::resetClearsBothCursors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CursorStore store(tempFilePath(dir, QStringLiteral("cursors.ini")));

    store.setMailCursor(QStringLiteral("100"));
    store.setContactBaseCursor(QStringLiteral("200"));
    QVERIFY(!store.mailCursor().isEmpty());
    QVERIFY(!store.contactBaseCursor().isEmpty());

    store.reset();
    QVERIFY(store.mailCursor().isEmpty());
    QVERIFY(store.contactBaseCursor().isEmpty());
}

QTEST_GUILESS_MAIN(CursorStoreTest)
#include "CursorStoreTest.moc"
