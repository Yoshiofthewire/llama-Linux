#include "pgp/PgpQrTargetValidator.h"

#include <QHostInfo>

namespace {

QList<QHostAddress> defaultResolveHost(const QString& host)
{
    QHostAddress literal;
    if (literal.setAddress(host))
        return { literal };
    // QHostInfo::fromName() is a real, blocking lookup (unlike
    // QHostInfo::lookupHost()'s async callback API) -- consistent with this
    // codebase's other synchronous-from-the-caller's-point-of-view network
    // calls (see HttpClient's own class doc comment).
    return QHostInfo::fromName(host).addresses();
}

} // namespace

bool isSafeQrTarget(const QUrl& url, const HostResolver& resolveHost)
{
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)
        return false;

    const QString host = url.host();
    if (host.isEmpty())
        return false;
    if (host.compare(QStringLiteral("metadata.google.internal"), Qt::CaseInsensitive) == 0)
        return false;

    const HostResolver resolver = resolveHost ? resolveHost : HostResolver(defaultResolveHost);
    const QList<QHostAddress> addresses = resolver(host);
    if (addresses.isEmpty())
        return false; // unresolvable -- can't verify safety, so don't proceed

    for (const QHostAddress& addr : addresses) {
        if (addr.isLinkLocal())
            return false;
    }

    return true;
}
