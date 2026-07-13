#include "net/RelayMailSource.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

// Appends the given API path to serverBaseUrl, mirroring ContactSyncClient's
// endpointFor -- preserves any existing path on serverBaseUrl and ensures
// exactly one slash between the two, regardless of whether the caller's base
// URL was given with or without a trailing slash.
QUrl endpointFor(const QUrl& serverBaseUrl, const QString& apiPath)
{
    QUrl url = serverBaseUrl;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    path += apiPath;
    url.setPath(path);
    return url;
}

QJsonDocument parseBody(const QByteArray& body, QJsonParseError* parseError)
{
    return QJsonDocument::fromJson(body, parseError);
}

// Maps one wire inbox item -- {messageId, sender, sentTo, cc, bcc, subject,
// body, status, atUtc, hasAttachments, label, detail?, changeType?} -- onto
// InboxEmailItem. atUtc is a direct pass-through (core/models/Email::atUtc
// already matches the wire key exactly, no casing translation). There is no
// distinct "preview" key on the wire, so Email::preview is left empty here
// (deferred, not guessed). folder is not itself a wire field on the item --
// it's set by the caller from the enclosing byTab map key.
InboxEmailItem inboxItemFromJson(const QJsonObject& obj)
{
    InboxEmailItem item;
    item.email.messageId = obj.value(QStringLiteral("messageId")).toString();
    item.email.sender = obj.value(QStringLiteral("sender")).toString();
    item.email.sentTo = obj.value(QStringLiteral("sentTo")).toString();
    item.email.cc = obj.value(QStringLiteral("cc")).toString();
    item.email.bcc = obj.value(QStringLiteral("bcc")).toString();
    item.email.subject = obj.value(QStringLiteral("subject")).toString();
    if (obj.contains(QStringLiteral("body")) && !obj.value(QStringLiteral("body")).isNull())
        item.email.body = obj.value(QStringLiteral("body")).toString();
    item.email.status = obj.value(QStringLiteral("status")).toString();
    item.email.atUtc = obj.value(QStringLiteral("atUtc")).toString();
    item.email.hasAttachments = obj.value(QStringLiteral("hasAttachments")).toBool();
    item.email.label = obj.value(QStringLiteral("label")).toString();

    if (obj.contains(QStringLiteral("detail")))
        item.detail = obj.value(QStringLiteral("detail")).toString();
    if (obj.contains(QStringLiteral("changeType")))
        item.changeType = obj.value(QStringLiteral("changeType")).toString();

    return item;
}

MailFolderItem folderItemFromJson(const QJsonObject& obj)
{
    MailFolderItem item;
    item.path = obj.value(QStringLiteral("path")).toString();
    item.deletable = obj.value(QStringLiteral("deletable")).toBool();
    return item;
}

ActionFailure actionFailureFromJson(const QJsonObject& obj)
{
    ActionFailure failure;
    failure.messageId = obj.value(QStringLiteral("messageId")).toString();
    failure.error = obj.value(QStringLiteral("error")).toString();
    return failure;
}

MailAttachmentInfo mailAttachmentInfoFromJson(const QJsonObject& obj)
{
    MailAttachmentInfo item;
    item.index = obj.value(QStringLiteral("index")).toInt();
    item.name = obj.value(QStringLiteral("name")).toString();
    item.mimeType = obj.value(QStringLiteral("mimeType")).toString();
    item.size = obj.value(QStringLiteral("size")).toInt();
    return item;
}

// Parses the filename out of a Content-Disposition header value shaped like
// `attachment; filename="<value>"`, with backslash-escaped quotes/
// backslashes inside the quoted string, per Go's mime.FormatMediaType (RFC
// 2045 quoted-string) -- confirmed as the only shape the backend emits, so
// this is deliberately not a general Content-Disposition/RFC 5987 parser.
QString filenameFromContentDisposition(const QString& headerValue)
{
    const QString marker = QStringLiteral("filename=\"");
    const int start = headerValue.indexOf(marker);
    if (start < 0)
        return {};

    QString filename;
    int i = start + marker.size();
    while (i < headerValue.size()) {
        const QChar ch = headerValue.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < headerValue.size()) {
            filename += headerValue.at(i + 1);
            i += 2;
            continue;
        }
        if (ch == QLatin1Char('"'))
            break;
        filename += ch;
        ++i;
    }
    return filename;
}

} // namespace

RelayMailSource::RelayMailSource(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

InboxFetchResult RelayMailSource::fetchInbox(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                              std::optional<int> limit, const QString& mailbox,
                                              std::optional<qint64> since) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    if (limit.has_value())
        query.append({ QStringLiteral("limit"), QString::number(*limit) });
    query.append({ QStringLiteral("mailbox"), mailbox });
    if (since.has_value())
        query.append({ QStringLiteral("since"), QString::number(*since) });

    const HttpClient::HttpResult result = m_httpClient.get(endpointFor(serverBaseUrl, QStringLiteral("api/inbox")), query);

    InboxFetchResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty() ? result.detail
                                               : QStringLiteral("Inbox fetch failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode inbox response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    for (const QJsonValue& tab : json.value(QStringLiteral("tabs")).toArray())
        out.tabs.append(tab.toString());

    const QJsonObject byTab = json.value(QStringLiteral("byTab")).toObject();
    for (auto it = byTab.constBegin(); it != byTab.constEnd(); ++it) {
        QVector<InboxEmailItem> items;
        const QJsonArray array = it.value().toArray();
        items.reserve(array.size());
        for (const QJsonValue& value : array) {
            InboxEmailItem item = inboxItemFromJson(value.toObject());
            item.email.folder = it.key();
            items.append(item);
        }
        out.byTab.insert(it.key(), items);
    }

    out.isDelta = json.value(QStringLiteral("delta")).toBool();
    out.cursor = static_cast<qint64>(json.value(QStringLiteral("cursor")).toDouble());
    for (const QJsonValue& value : json.value(QStringLiteral("removed")).toArray())
        out.removed.append(value.toString());

    return out;
}

ListFoldersResult RelayMailSource::listFolders(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                const QString& parent) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("parent"), parent });

    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, QStringLiteral("api/inbox/folders")), query);

    ListFoldersResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Folder list failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode folder list response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.parent = json.value(QStringLiteral("parent")).toString();
    for (const QJsonValue& value : json.value(QStringLiteral("folders")).toArray())
        out.folders.append(folderItemFromJson(value.toObject()));

    return out;
}

CreateFolderResult RelayMailSource::createFolder(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                   const QString& parent, const QString& name) const
{
    QJsonObject body;
    body[QStringLiteral("parent")] = parent;
    body[QStringLiteral("name")] = name;

    const HttpClient::HttpResult result =
        m_httpClient.post(endpointFor(serverBaseUrl, QStringLiteral("api/inbox/folders")), auth.queryItems(), body);

    CreateFolderResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Folder create failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode folder create response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.ok = json.value(QStringLiteral("ok")).toBool();
    out.parent = json.value(QStringLiteral("parent")).toString();
    out.name = json.value(QStringLiteral("name")).toString();
    out.folder = json.value(QStringLiteral("folder")).toString();
    return out;
}

RenameFolderResult RelayMailSource::renameFolder(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                   const QString& folder, const QString& name) const
{
    QJsonObject body;
    body[QStringLiteral("folder")] = folder;
    body[QStringLiteral("name")] = name;

    const HttpClient::HttpResult result =
        m_httpClient.put(endpointFor(serverBaseUrl, QStringLiteral("api/inbox/folders")), auth.queryItems(), body);

    RenameFolderResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Folder rename failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode folder rename response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.ok = json.value(QStringLiteral("ok")).toBool();
    out.folder = json.value(QStringLiteral("folder")).toString();
    out.renamed = json.value(QStringLiteral("renamed")).toString();
    out.parent = json.value(QStringLiteral("parent")).toString();
    return out;
}

DeleteFolderResult RelayMailSource::deleteFolder(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                   const QString& folder) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("folder"), folder });

    const HttpClient::HttpResult result =
        m_httpClient.del(endpointFor(serverBaseUrl, QStringLiteral("api/inbox/folders")), query);

    DeleteFolderResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Folder delete failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode folder delete response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.ok = json.value(QStringLiteral("ok")).toBool();
    out.folder = json.value(QStringLiteral("folder")).toString();
    out.parent = json.value(QStringLiteral("parent")).toString();
    return out;
}

ActionResult RelayMailSource::performAction(const QUrl& serverBaseUrl, const RelayAuth& auth, const QString& action,
                                             const QStringList& messageIds, const QString& mailbox,
                                             const std::optional<QString>& targetMailbox) const
{
    QJsonObject body;
    body[QStringLiteral("action")] = action;
    body[QStringLiteral("messageIds")] = QJsonArray::fromStringList(messageIds);
    body[QStringLiteral("mailbox")] = mailbox;
    if (targetMailbox.has_value())
        body[QStringLiteral("targetMailbox")] = *targetMailbox;

    const HttpClient::HttpResult result =
        m_httpClient.post(endpointFor(serverBaseUrl, QStringLiteral("api/inbox/actions")), auth.queryItems(), body);

    ActionResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Inbox action failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode inbox action response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.ok = json.value(QStringLiteral("ok")).toBool();
    out.action = json.value(QStringLiteral("action")).toString();
    out.processed = json.value(QStringLiteral("processed")).toInt();
    out.targetMailbox = json.value(QStringLiteral("targetMailbox")).toString();
    for (const QJsonValue& value : json.value(QStringLiteral("failed")).toArray())
        out.failed.append(actionFailureFromJson(value.toObject()));

    return out;
}

SendMailResult RelayMailSource::sendMail(const QUrl& serverBaseUrl, const RelayAuth& auth, const QString& to,
                                          const QString& cc, const QString& bcc, const QString& subject,
                                          const QString& body, const QString& mode,
                                          const QVector<MailAttachmentUpload>& attachments) const
{
    QJsonArray attachmentsJson;
    for (const MailAttachmentUpload& attachment : attachments) {
        QJsonObject attachmentJson;
        attachmentJson[QStringLiteral("name")] = attachment.name;
        attachmentJson[QStringLiteral("mimeType")] = attachment.mimeType;
        attachmentJson[QStringLiteral("dataBase64")] = QString::fromLatin1(attachment.data.toBase64());
        attachmentsJson.append(attachmentJson);
    }

    QJsonObject requestBody;
    requestBody[QStringLiteral("to")] = to;
    requestBody[QStringLiteral("cc")] = cc;
    requestBody[QStringLiteral("bcc")] = bcc;
    requestBody[QStringLiteral("subject")] = subject;
    requestBody[QStringLiteral("body")] = body;
    requestBody[QStringLiteral("mode")] = mode;
    requestBody[QStringLiteral("attachments")] = attachmentsJson;

    const HttpClient::HttpResult result =
        m_httpClient.post(endpointFor(serverBaseUrl, QStringLiteral("api/mail/send")), auth.queryItems(), requestBody);

    SendMailResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty() ? result.detail
                                               : QStringLiteral("Mail send failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode mail send response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.ok = json.value(QStringLiteral("ok")).toBool();
    out.sentSaved = json.value(QStringLiteral("sentSaved")).toBool();
    out.warning = json.value(QStringLiteral("warning")).toString();
    return out;
}

ListAttachmentsResult RelayMailSource::listAttachments(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                         const QString& mailbox, const QString& messageId) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });

    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, QStringLiteral("api/mail/attachments")), query);

    ListAttachmentsResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Attachment list failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = parseBody(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode attachment list response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    for (const QJsonValue& value : json.value(QStringLiteral("attachments")).toArray())
        out.attachments.append(mailAttachmentInfoFromJson(value.toObject()));

    return out;
}

DownloadAttachmentResult RelayMailSource::downloadAttachment(const QUrl& serverBaseUrl, const RelayAuth& auth,
                                                               const QString& mailbox, const QString& messageId,
                                                               int index) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });
    query.append({ QStringLiteral("index"), QString::number(index) });

    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, QStringLiteral("api/mail/attachment")), query);

    DownloadAttachmentResult out;
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Attachment download failed with status %1").arg(result.statusCode);
        return out;
    }

    // Not JSON -- the body is the attachment's raw bytes verbatim, filename/
    // mime type come from the response headers instead (see
    // filenameFromContentDisposition above).
    out.data = result.body;
    for (const auto& header : result.headers) {
        if (header.first.compare(QStringLiteral("Content-Type"), Qt::CaseInsensitive) == 0)
            out.mimeType = header.second;
        else if (header.first.compare(QStringLiteral("Content-Disposition"), Qt::CaseInsensitive) == 0)
            out.filename = filenameFromContentDisposition(header.second);
    }
    return out;
}
