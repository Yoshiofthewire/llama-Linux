#pragma once

#include "models/PushNotification.h"

#include <QByteArray>
#include <optional>

// Parses the raw UnifiedPush message body (JSON bytes, as delivered by
// KUnifiedPush::Connector::messageReceived -- see UnifiedPushConnector.cpp)
// into a PushNotification. Pure function over bytes, no QObject needed, so
// this is a plain namespace rather than a class -- matches the task-40 brief.
//
// Verified wire envelope (backend/internal/processor/native_sender.go +
// poller.go's buildNativePushData, cross-checked in
// .superpowers/sdd/phase7-global-constraints.md):
//   { "title": "...", "body": "...",
//     "data": { "messageId", "sender", "subject", "senderName",
//               "emailSubject", "Keywords" (comma-joined, capital K),
//               "title", "body", "url" } }
//
// PushNotification::title/body are populated from data.title/data.body when
// present, falling back to the outer envelope's title/body otherwise. The
// outer pair exists purely for distributor/OS-tray display; data's copy is
// preferred as the source of truth for a real mail push. (This is the
// opposite convention from PushNotificationClient.cpp's pull-mode parsing,
// which takes title/body from the item's top level -- that envelope shape
// has no data.title/data.body duplicate. Two different wire shapes,
// deliberately handled differently; see PushNotificationClient.cpp's own
// comment.)
//
// Real mail pushes always carry data.messageId (the identity key every
// other piece of this repo -- PushDao/PushRepository::recordPushArrival --
// treats as required for persistence/dedup). The backend's other native
// push producer, POST /api/notifications/test (server.go's
// handleNotificationTest, used by the web app's "Send Test Notification"
// button), sends a distinctly sparser envelope with no data.messageId at
// all -- only an outer title/body and a bare data.url. This parser accepts
// both real shapes: it only rejects a payload with genuinely nothing
// displayable (empty messageId AND empty title after the data/outer
// fallback above). Callers that persist/dedupe on messageId (see
// main.cpp's distributor-tier lambda) must still guard on messageId being
// non-empty themselves -- an accepted result here is not a guarantee of a
// real mail identity.
namespace PushPayloadParser {

// Returns std::nullopt on malformed JSON, a non-object top level, or a
// payload with neither a real data.messageId nor any displayable title
// (checked after the data/outer title fallback described above).
std::optional<PushNotification> parse(const QByteArray& rawBody);

} // namespace PushPayloadParser
