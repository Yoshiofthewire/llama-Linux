#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

// Resolves a hostname to its IP addresses -- injectable so tests can
// exercise the link-local check against a hostname without depending on
// real DNS. Defaults to a real (blocking) lookup in production; see
// PgpQrTargetValidator.cpp's defaultResolveHost(). Literal IP strings are
// handled by the default resolver without any actual DNS query.
using HostResolver = std::function<QList<QHostAddress>(const QString& host)>;

// Blocks the two classes of scanQrPayload() target that would turn a QR
// scan into something worse than "fetch a key from wherever it points":
// non-http(s) schemes (file:// would read local files back as if they were
// key material) and link-local/cloud-metadata addresses (169.254.0.0/16 --
// AWS/Azure/DigitalOcean's metadata IP, plus GCP's metadata.google.internal
// hostname). Arbitrary *http(s)* hosts, including LAN/private IPs and
// loopback, are intentionally still allowed -- scanQrPayload()'s own
// comment notes the scanned server is expected to often be a different,
// independently self-hosted Relay instance than this device's own paired
// one, and self-hosted instances commonly live on a home LAN or even
// localhost.
//
// VibeSec finding: the link-local check used to only fire when the QR
// text's host was ALREADY a literal IP string -- QHostAddress::setAddress()
// returns false for an ordinary hostname, so the check silently
// short-circuited and any attacker-registered domain whose DNS A record
// pointed at a link-local/metadata address sailed straight through. This
// now resolves every host (via resolveHost, real DNS by default) and
// checks every resolved address, closing that gap. An unresolvable host is
// rejected (fail closed) rather than treated as safe.
bool isSafeQrTarget(const QUrl& url, const HostResolver& resolveHost = {});
