#include "net/RelayAuth.h"

#include <QTest>

class RelayAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void headerItemsReturnsSubscriberIdAndHashAsNamedHeaders();
};

void RelayAuthTest::headerItemsReturnsSubscriberIdAndHashAsNamedHeaders()
{
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };

    const QList<QPair<QString, QString>> items = auth.headerItems();

    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).first, QStringLiteral("X-Kypost-Subscriber-Id"));
    QCOMPARE(items.at(0).second, QStringLiteral("sub-1"));
    QCOMPARE(items.at(1).first, QStringLiteral("X-Kypost-Subscriber-Hash"));
    QCOMPARE(items.at(1).second, QStringLiteral("hash-1"));
}

QTEST_GUILESS_MAIN(RelayAuthTest)
#include "RelayAuthTest.moc"
