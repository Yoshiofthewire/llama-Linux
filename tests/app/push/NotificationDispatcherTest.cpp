#include "push/NotificationDispatcher.h"

#include "models/PushNotification.h"

#include <QTest>

// Covers only NotificationDispatcher::pickTitle/pickText -- the pure,
// deterministic fallback-selection logic behind notify()'s title/text. Does
// not (and cannot) exercise notify() itself: that requires a real
// KNotification/D-Bus round-trip, which is out of scope for a unit test --
// see tests/CMakeLists.txt's Task-40 comment.
class NotificationDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void titleUsesSenderNameWhenPresent();
    void titleFallsBackToSenderWhenSenderNameEmpty();
    void titleFallsBackToTitleWhenSenderNameAndSenderEmpty();
    void titleIsEmptyWhenAllThreeEmpty();

    void textUsesEmailSubjectWhenPresent();
    void textFallsBackToSubjectWhenEmailSubjectEmpty();
    void textFallsBackToBodyWhenEmailSubjectAndSubjectEmpty();
    void textIsEmptyWhenAllThreeEmpty();
};

void NotificationDispatcherTest::titleUsesSenderNameWhenPresent()
{
    PushNotification payload;
    payload.senderName = QStringLiteral("Alice");
    payload.sender = QStringLiteral("a@example.com");

    QCOMPARE(NotificationDispatcher::pickTitle(payload), QStringLiteral("Alice"));
}

void NotificationDispatcherTest::titleFallsBackToSenderWhenSenderNameEmpty()
{
    PushNotification payload;
    payload.senderName.clear();
    payload.sender = QStringLiteral("a@example.com");

    QCOMPARE(NotificationDispatcher::pickTitle(payload), QStringLiteral("a@example.com"));
}

void NotificationDispatcherTest::titleFallsBackToTitleWhenSenderNameAndSenderEmpty()
{
    // Covers the Task 43 review-finding fix: the EmbeddedSubscriber tier
    // (main.cpp's NtfySubscriber-arrival lambda) only ever populates
    // payload.title/payload.body, never senderName/sender.
    PushNotification payload;
    payload.senderName.clear();
    payload.sender.clear();
    payload.title = QStringLiteral("ntfy title");

    QCOMPARE(NotificationDispatcher::pickTitle(payload), QStringLiteral("ntfy title"));
}

void NotificationDispatcherTest::titleIsEmptyWhenAllThreeEmpty()
{
    PushNotification payload;
    payload.senderName.clear();
    payload.sender.clear();
    payload.title.clear();

    QVERIFY(NotificationDispatcher::pickTitle(payload).isEmpty());
}

void NotificationDispatcherTest::textUsesEmailSubjectWhenPresent()
{
    PushNotification payload;
    payload.emailSubject = QStringLiteral("Hello there");
    payload.subject = QStringLiteral("Hello");

    QCOMPARE(NotificationDispatcher::pickText(payload), QStringLiteral("Hello there"));
}

void NotificationDispatcherTest::textFallsBackToSubjectWhenEmailSubjectEmpty()
{
    PushNotification payload;
    payload.emailSubject.clear();
    payload.subject = QStringLiteral("Hello");

    QCOMPARE(NotificationDispatcher::pickText(payload), QStringLiteral("Hello"));
}

void NotificationDispatcherTest::textFallsBackToBodyWhenEmailSubjectAndSubjectEmpty()
{
    // Covers the Task 43 review-finding fix -- see
    // titleFallsBackToTitleWhenSenderNameAndSenderEmpty()'s comment above.
    PushNotification payload;
    payload.emailSubject.clear();
    payload.subject.clear();
    payload.body = QStringLiteral("ntfy message body");

    QCOMPARE(NotificationDispatcher::pickText(payload), QStringLiteral("ntfy message body"));
}

void NotificationDispatcherTest::textIsEmptyWhenAllThreeEmpty()
{
    PushNotification payload;
    payload.emailSubject.clear();
    payload.subject.clear();
    payload.body.clear();

    QVERIFY(NotificationDispatcher::pickText(payload).isEmpty());
}

QTEST_GUILESS_MAIN(NotificationDispatcherTest)
#include "NotificationDispatcherTest.moc"
