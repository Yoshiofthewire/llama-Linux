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
    if (!payload.senderName.isEmpty())
        return payload.senderName;
    if (!payload.sender.isEmpty())
        return payload.sender;
    // Task 43 review-finding fix: last-resort fallback for tiers (currently
    // only EmbeddedSubscriber, via NtfySubscriber's flat {title,message}
    // envelope) that never populate senderName/sender at all. See the
    // header comment for why this never changes the Distributor tier's
    // already-correct rendering.
    return payload.title;
}

QString NotificationDispatcher::pickText(const PushNotification& payload)
{
    if (!payload.emailSubject.isEmpty())
        return payload.emailSubject;
    if (!payload.subject.isEmpty())
        return payload.subject;
    return payload.body;
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
