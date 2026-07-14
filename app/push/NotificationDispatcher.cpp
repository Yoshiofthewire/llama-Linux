#include "push/NotificationDispatcher.h"

#include "models/PushNotification.h"

#include <KNotification>

#include <QDebug>

NotificationDispatcher::NotificationDispatcher(QObject* parent)
    : QObject(parent)
{
}

QString NotificationDispatcher::pickTitle(const PushNotification& payload)
{
    return !payload.senderName.isEmpty() ? payload.senderName : payload.sender;
}

QString NotificationDispatcher::pickText(const PushNotification& payload)
{
    return !payload.emailSubject.isEmpty() ? payload.emailSubject : payload.subject;
}

void NotificationDispatcher::notify(const PushNotification& payload)
{
    // Logging discipline (Phase 7 global constraint 6): never log
    // payload.body/subject/senderName/emailSubject content, only messageId
    // (an opaque identifier, not message content) plus a bare arrival
    // marker.
    qDebug() << "NotificationDispatcher: notifying for messageId" << payload.messageId;

    const QString title = pickTitle(payload);
    const QString text = pickText(payload);

    // No parent: matches KNotification's own documented lifecycle -- with
    // the default CloseOnTimeout flag it deletes itself once the
    // notification closes.
    auto* notification = new KNotification(QStringLiteral("newMail"));
    notification->setTitle(title);
    notification->setText(text);

    const QString messageId = payload.messageId;
    KNotificationAction* viewAction = notification->addDefaultAction(QStringLiteral("View"));
    connect(viewAction, &KNotificationAction::activated, this, [this, messageId]() {
        emit openRequested(messageId);
    });

    notification->sendEvent();
}
