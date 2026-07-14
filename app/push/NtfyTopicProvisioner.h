#pragma once

#include <QString>

class SecureStore;

// Generates and persists this device's ntfy topic -- the bearer secret for
// the EmbeddedSubscriber push tier (core/net/NtfySubscriber's long-poll
// subscription against SettingsStore::pushServerBaseUrl(), default
// https://ntfy.sh). A topic doubles as a bearer credential on this path
// (Linux_QT_Client_Plan.md's risk #8: anyone who knows the topic string can
// read this device's ntfy stream), so it must carry >=128 bits of entropy
// and must never be logged -- see phase7-global-constraints.md item 6's
// logging-discipline rule, which callers (main.cpp) must also respect.
//
// Deliberately app/ layer, not core/: SecureStore is a core/ interface, but
// deciding *how* to generate the topic and wiring it into NtfySubscriber's
// constructor both belong at the composition root, same as PairingStore's
// own SecureStore key-string convention (core/domain/PairingStore.cpp) --
// this fills in SecureStore.h's own aspirational "ntfy-topic" key doc
// comment, which nothing implemented until now (Task 43 review finding).
namespace NtfyTopicProvisioner {

// Returns the persisted topic if one already exists (SecureStore key
// "ntfy-topic"); otherwise generates a fresh one, persists it, and returns
// it. Call once at composition-root time, before constructing NtfySubscriber
// with the result.
QString getOrCreateTopic(SecureStore& secureStore);

// Unconditionally generates a fresh topic and persists it, overwriting any
// existing one. Intended for a re-pair hook (risk #8: "rotated on
// re-pair") -- note this only updates the *persisted* secret. An
// already-running NtfySubscriber instance keeps using the topic it was
// constructed with (NtfySubscriber has no live topic-update seam, and
// adding one would touch its core logic, out of this task's scope), so a
// rotation here takes effect starting from the next app launch, not
// mid-session.
QString rotateTopic(SecureStore& secureStore);

} // namespace NtfyTopicProvisioner
