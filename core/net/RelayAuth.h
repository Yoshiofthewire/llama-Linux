#pragma once

#include <QList>
#include <QPair>
#include <QString>

// Relay auth credentials (subscriber id + hash). headerItems() sends them
// as X-Kypost-Subscriber-Id/X-Kypost-Subscriber-Hash headers -- the form
// every Relay call site is migrating to (server already accepts both
// forms, headers preferred). queryItems() (legacy ?sub=&hash= query
// params) is being phased out call site by call site and deleted once
// nothing uses it anymore. Mirrors llama-Mail-for-Mac's RelayAuth
// (Data/Networking/HTTPClient.swift), read for structure only. Plain value
// type with no store dependency -- callers pull sub/hash out of whatever
// pairing/session store owns them and hand this to HttpClient::get/post.
struct RelayAuth
{
    QString sub;
    QString hash;

    QList<QPair<QString, QString>> queryItems() const
    {
        return { { QStringLiteral("sub"), sub }, { QStringLiteral("hash"), hash } };
    }

    QList<QPair<QString, QString>> headerItems() const
    {
        return { { QStringLiteral("X-Kypost-Subscriber-Id"), sub },
                 { QStringLiteral("X-Kypost-Subscriber-Hash"), hash } };
    }

    bool operator==(const RelayAuth&) const = default;
};
