#pragma once

#include <QObject>
#include <QString>

struct PushNotification;

// Wraps KNotification (KNotifications/KNotification, KF6::Notifications) to
// surface a parsed PushNotification as a desktop notification. Lives in
// app/push/, alongside UnifiedPushConnector, per the existing core/ boundary
// rule -- KNotifications is a KF6 system library, never linked into
// llamacore.
//
// Deliberately ignorant of QML/MailController/window focus: Task 42 wires
// openRequested() to real navigation. This class only builds and sends the
// KNotification and forwards its default-action activation as a Qt signal.
class NotificationDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit NotificationDispatcher(QObject* parent = nullptr);

    // Builds and sends a KNotification for payload, with one default
    // ("View") action. Title/text field choice: payload.senderName
    // (falling back to payload.sender when empty) as the title,
    // payload.emailSubject (falling back to payload.subject when empty) as
    // the text -- matches buildNativePushData's own field-naming intent
    // that senderName/emailSubject are the friendlier display copies, while
    // sender/subject are the raw fallbacks for when those are absent. Built
    // via pickTitle()/pickText() below.
    void notify(const PushNotification& payload);

    // Pure, deterministic fallback-selection logic backing notify()'s
    // title/text. Public and static -- rather than anonymous-namespace
    // file-scope helpers, the way PushPayloadParser.cpp's splitKeywords is
    // done -- specifically so NotificationDispatcherTest can call them
    // directly without touching KNotification/D-Bus. notify() itself has no
    // fake/injectable seam to test through (see tests/CMakeLists.txt's
    // Task-40 comment on why NotificationDispatcher has no test exercising
    // notify() end-to-end), so these two functions are this class's only
    // unit-testable surface.
    static QString pickTitle(const PushNotification& payload);
    static QString pickText(const PushNotification& payload);

signals:
    // Emitted when the user activates the notification's default ("View")
    // action.
    void openRequested(const QString& messageId);
};
