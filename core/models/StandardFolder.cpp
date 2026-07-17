#include "models/StandardFolder.h"

#include <QStringList>

QString standardFolderWireName(StandardFolder folder)
{
    switch (folder) {
    case StandardFolder::Inbox:
        return QStringLiteral("INBOX");
    case StandardFolder::Drafts:
        return QStringLiteral("Drafts");
    case StandardFolder::Junk:
        return QStringLiteral("Junk");
    case StandardFolder::Sent:
        return QStringLiteral("Sent");
    case StandardFolder::Trash:
        return QStringLiteral("Trash");
    case StandardFolder::Archive:
        return QStringLiteral("Archive");
    }
    return QString();
}

QString standardFolderDisplayName(const QString& fullPath)
{
    const QString normalized = QString(fullPath).replace(QLatin1Char('.'), QLatin1Char('/'));
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? fullPath : parts.last();
}
