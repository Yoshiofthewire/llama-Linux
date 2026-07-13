#pragma once

#include "net/NetworkError.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

// Synchronous-from-the-caller's-point-of-view wrapper around
// QNetworkAccessManager for Relay HTTP calls. get()/post() block the calling
// thread on a local QEventLoop driven by the reply's finished signal — this
// mirrors llama-Mail-for-Mac's HTTPClient async/await call shape one-for-one
// (verified reference: Data/Networking/HTTPClient.swift, read for structure
// only), so every Task 14-18 client reads as a straight-line sequence
// instead of a signal/callback chain. Callers must invoke get()/post() off
// the GUI thread once app/ wiring exists in a later phase.
//
// The QNetworkAccessManager is injected via constructor reference rather
// than default-constructed internally, so tests can point it at a local
// QTcpServer and so threading/lifetime ownership stays with the caller.
class HttpClient
{
public:
    struct HttpResult
    {
        std::optional<NetworkError> error;
        int statusCode = 0;
        QByteArray body;
        QString detail; // human-readable detail for Transport/InvalidUrl failures; empty otherwise
        // Response headers as received from the server, populated by all four
        // verb methods below (via waitForReply). Added in Task 18 for the
        // attachment-download endpoint, whose filename/mime type travel in
        // Content-Disposition/Content-Type rather than the JSON body -- empty
        // for the InvalidUrl early-return path, where no reply was made.
        QList<QPair<QString, QString>> headers;
    };

    // transferTimeoutMs guards waitForReply()'s QEventLoop against a
    // hung/silent server that never emits QNetworkReply::finished (no
    // response, no error) -- without it, the calling thread blocks forever.
    // Only applied to the injected manager if it doesn't already have a
    // transfer timeout configured (manager.transferTimeout() == 0), so a
    // caller's own configuration is never clobbered. Exposed as a
    // constructor parameter (rather than hardcoded) so tests can pass a
    // short override instead of waiting out the real default.
    explicit HttpClient(QNetworkAccessManager& manager, int transferTimeoutMs = 30000);

    // HttpResult never decodes JSON: decoding into a concrete struct is each
    // Task 14-18 client's own responsibility (QJsonDocument::fromJson on
    // HttpResult::body, mapping a QJsonParseError to NetworkError::Decoding
    // if error is unset here but parsing still fails).
    HttpResult get(const QUrl& url, const QList<QPair<QString, QString>>& query,
                   const QList<QPair<QString, QString>>& headers = {});

    // Sets Content-Type: application/json.
    HttpResult post(const QUrl& url, const QList<QPair<QString, QString>>& query,
                     const QJsonObject& jsonBody, const QList<QPair<QString, QString>>& headers = {});

    // Sets Content-Type: application/json. Mirrors post()'s signature shape
    // -- added in Task 17 for the folder-rename endpoint.
    HttpResult put(const QUrl& url, const QList<QPair<QString, QString>>& query,
                    const QJsonObject& jsonBody, const QList<QPair<QString, QString>>& headers = {});

    // No body -- mirrors get()'s signature shape. Added in Task 17 for the
    // folder-delete endpoint, which takes its target via query param.
    HttpResult del(const QUrl& url, const QList<QPair<QString, QString>>& query,
                    const QList<QPair<QString, QString>>& headers = {});

private:
    // Appends query items to url via QUrlQuery, preserving any query url
    // already has — mirrors the Swift URL.appending(queryOrThrow:) extension.
    QUrl urlWithQuery(const QUrl& url, const QList<QPair<QString, QString>>& query) const;

    HttpResult waitForReply(QNetworkReply* reply) const;

    QNetworkAccessManager& m_manager;
};
