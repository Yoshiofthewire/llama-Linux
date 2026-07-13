#pragma once

#include "models/PushNotification.h"
#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <QVector>
#include <optional>

class HttpClient;
struct RelayAuth;

// One entry from the pull-mode response's "notifications" array. seq is not
// itself a PushNotification field — callers need it to advance their cursor
// on the next pull() call, so it travels alongside the mapped notification
// rather than being folded into the model.
struct PullNotificationItem
{
    qint64 seq = 0;
    PushNotification notification;

    bool operator==(const PullNotificationItem&) const = default;
};

// error/detail follow the same passthrough pattern as HttpClient::HttpResult
// (see NetworkError.h): error is unset only on a fully successful pull, so a
// failed request is always distinguishable from a successful pull that
// simply returned zero notifications.
struct PullResult
{
    std::optional<NetworkError> error;
    QString detail;
    QString deliveryMode; // "push" or "pull"
    qint64 cursor = 0;
    QVector<PullNotificationItem> notifications;
};

// Pull-mode fallback client for GET /api/notifications/native/pull, verified
// against internal/api/server.go's handleNotificationNativePull and
// internal/state/store.go's PullNotification struct (see Task 14 brief) —
// the Swift reference client's flattened PullNotificationDTO is stale and
// was not used as a source for this shape.
class PushNotificationClient
{
public:
    explicit PushNotificationClient(HttpClient& httpClient);

    // afterCursor <= 0 means "first pull" and is sent as omitted from the
    // query (the server only honors positive values); afterCursor > 0 is
    // sent as the "after" query param.
    PullResult pull(const QUrl& pullEndpoint, const RelayAuth& auth, qint64 afterCursor) const;

private:
    HttpClient& m_httpClient;
};
