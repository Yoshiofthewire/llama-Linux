#include "net/RelayMailSource.h"

#include "models/Email.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTest>

class RelayMailSourceTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchInboxMapsTwoTabsWithAtUtcPassthroughAndOptionalFields();
    void fetchInboxSendsLimitMailboxSinceAndAuthAsQueryParams();
    void fetchInboxOmitsLimitAndSinceWhenNotProvided();
    void fetchInboxUnauthorizedFrom401PassesErrorThrough();

    void listFoldersSendsParentAndAuthAsQueryParamsAndParsesResult();
    void createFolderSendsParentNameBodyAndParsesStringFolderResult();
    void renameFolderSendsPutWithFolderNameBodyAndParsesResult();
    void deleteFolderSendsDeleteWithFolderQueryParamNotBody();

    void performActionMoveIncludesTargetMailboxInRequestBody();
    void performActionReadOmitsTargetMailboxFromRequestBodyButResponseCarriesEmptyString();

    void sendMailJoinsRecipientsAndBase64EncodesAttachmentByteForByte();
    void sendMailSendsEmptyAttachmentsArrayWhenNoneProvided();
    void sendMailParsesAlwaysPresentWarningField();

    void listAttachmentsSendsMailboxMessageIdAndAuthAsQueryParamsAndParsesResult();

    void downloadAttachmentReturnsRawBytesAndParsesFilenameFromContentDisposition();
    void downloadAttachmentMapsNotFoundFrom404();
};

void RelayMailSourceTest::fetchInboxMapsTwoTabsWithAtUtcPassthroughAndOptionalFields()
{
    const QByteArray body = R"(
    {
      "tabs": ["Inbox", "Archive"],
      "byTab": {
        "Inbox": [
          {
            "messageId": "m1",
            "sender": "alice@example.com",
            "sentTo": "bob@example.com",
            "cc": "cc@example.com",
            "bcc": "bcc@example.com",
            "subject": "Hello",
            "body": "Body text",
            "status": "unread",
            "atUtc": "2026-07-01T12:00:00Z",
            "hasAttachments": true,
            "label": "important",
            "detail": "queued",
            "changeType": "updated"
          }
        ],
        "Archive": [
          {
            "messageId": "m2",
            "sender": "carol@example.com",
            "sentTo": "dave@example.com",
            "cc": "",
            "bcc": "",
            "subject": "Archived",
            "status": "read",
            "atUtc": "2026-06-01T08:30:00Z",
            "hasAttachments": false,
            "label": ""
          }
        ]
      }
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const InboxFetchResult result = source.fetchInbox(serverBaseUrl, auth, 100, QStringLiteral("Inbox"), qint64(0));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.tabs, QStringList({ QStringLiteral("Inbox"), QStringLiteral("Archive") }));
    QCOMPARE(result.byTab.size(), 2);

    QVERIFY(result.byTab.contains(QStringLiteral("Inbox")));
    QCOMPARE(result.byTab.value(QStringLiteral("Inbox")).size(), 1);
    const InboxEmailItem& item1 = result.byTab.value(QStringLiteral("Inbox")).at(0);
    QCOMPARE(item1.email.messageId, QStringLiteral("m1"));
    QCOMPARE(item1.email.sender, QStringLiteral("alice@example.com"));
    QCOMPARE(item1.email.sentTo, QStringLiteral("bob@example.com"));
    QCOMPARE(item1.email.cc, QStringLiteral("cc@example.com"));
    QCOMPARE(item1.email.bcc, QStringLiteral("bcc@example.com"));
    QCOMPARE(item1.email.subject, QStringLiteral("Hello"));
    QVERIFY(item1.email.body.has_value());
    QCOMPARE(*item1.email.body, QStringLiteral("Body text"));
    // No distinct "preview" key exists on the wire (confirmed against the Go
    // backend) -- preview must stay empty, not be populated from body.
    QVERIFY(item1.email.preview.isEmpty());
    QCOMPARE(item1.email.status, QStringLiteral("unread"));
    // atUtc is a direct pass-through of the wire key of the same name -- no
    // casing translation, assert it is unchanged.
    QCOMPARE(item1.email.atUtc, QStringLiteral("2026-07-01T12:00:00Z"));
    QCOMPARE(item1.email.hasAttachments, true);
    QCOMPARE(item1.email.label, QStringLiteral("important"));
    // folder is set from the enclosing byTab map key, not a wire field.
    QCOMPARE(item1.email.folder, QStringLiteral("Inbox"));
    QCOMPARE(item1.detail, QStringLiteral("queued"));
    QVERIFY(item1.changeType.has_value());
    QCOMPARE(*item1.changeType, QStringLiteral("updated"));

    QVERIFY(result.byTab.contains(QStringLiteral("Archive")));
    QCOMPARE(result.byTab.value(QStringLiteral("Archive")).size(), 1);
    const InboxEmailItem& item2 = result.byTab.value(QStringLiteral("Archive")).at(0);
    QCOMPARE(item2.email.messageId, QStringLiteral("m2"));
    QCOMPARE(item2.email.folder, QStringLiteral("Archive"));
    // "body"/"detail"/"changeType" absent from the wire -> nullopt/empty, not
    // a parse error.
    QVERIFY(!item2.email.body.has_value());
    QVERIFY(item2.detail.isEmpty());
    QVERIFY(!item2.changeType.has_value());
}

void RelayMailSourceTest::fetchInboxSendsLimitMailboxSinceAndAuthAsQueryParams()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"tabs":[],"byTab":{}})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    source.fetchInbox(serverBaseUrl, auth, 250, QStringLiteral("Inbox"), qint64(12345));

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox?"));
    QVERIFY(request.contains("sub=sub-9"));
    QVERIFY(request.contains("hash=hash-9"));
    QVERIFY(request.contains("limit=250"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("since=12345"));
}

void RelayMailSourceTest::fetchInboxOmitsLimitAndSinceWhenNotProvided()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"tabs":[],"byTab":{}})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    source.fetchInbox(serverBaseUrl, auth, std::nullopt, QStringLiteral("Inbox"), std::nullopt);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(!request.contains("limit="));
    QVERIFY(!request.contains("since="));
    QVERIFY(request.contains("mailbox=Inbox"));
}

void RelayMailSourceTest::fetchInboxUnauthorizedFrom401PassesErrorThrough()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const InboxFetchResult result = source.fetchInbox(serverBaseUrl, auth, std::nullopt, QStringLiteral("Inbox"), std::nullopt);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.byTab.isEmpty());
}

void RelayMailSourceTest::listFoldersSendsParentAndAuthAsQueryParamsAndParsesResult()
{
    const QByteArray body = R"(
    {
      "parent": "Inbox",
      "folders": [
        {"path": "Inbox/Work", "deletable": true},
        {"path": "Inbox/Personal", "deletable": false}
      ]
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ListFoldersResult result = source.listFolders(serverBaseUrl, auth, QStringLiteral("Inbox"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.parent, QStringLiteral("Inbox"));
    QCOMPARE(result.folders.size(), 2);
    QCOMPARE(result.folders.at(0).path, QStringLiteral("Inbox/Work"));
    QCOMPARE(result.folders.at(0).deletable, true);
    QCOMPARE(result.folders.at(1).path, QStringLiteral("Inbox/Personal"));
    QCOMPARE(result.folders.at(1).deletable, false);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox/folders?"));
    QVERIFY(request.contains("parent=Inbox"));
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
}

void RelayMailSourceTest::createFolderSendsParentNameBodyAndParsesStringFolderResult()
{
    // POST response's "folder" key is a plain string path here (mailClient.
    // CreateFolder returns (string, error)) -- NOT the {path, deletable}
    // object shape used by the GET list response.
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"parent":"Inbox","name":"NewFolder","folder":"Inbox/NewFolder"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const CreateFolderResult result =
        source.createFolder(serverBaseUrl, auth, QStringLiteral("Inbox"), QStringLiteral("NewFolder"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.parent, QStringLiteral("Inbox"));
    QCOMPARE(result.name, QStringLiteral("NewFolder"));
    QCOMPARE(result.folder, QStringLiteral("Inbox/NewFolder"));

    QVERIFY(fake.receivedRequest().contains("POST /api/inbox/folders?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("parent")).toString(), QStringLiteral("Inbox"));
    QCOMPARE(sent.value(QStringLiteral("name")).toString(), QStringLiteral("NewFolder"));
}

void RelayMailSourceTest::renameFolderSendsPutWithFolderNameBodyAndParsesResult()
{
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"folder":"Inbox/Old","renamed":"Inbox/New","parent":"Inbox"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const RenameFolderResult result =
        source.renameFolder(serverBaseUrl, auth, QStringLiteral("Inbox/Old"), QStringLiteral("New"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.folder, QStringLiteral("Inbox/Old"));
    QCOMPARE(result.renamed, QStringLiteral("Inbox/New"));
    QCOMPARE(result.parent, QStringLiteral("Inbox"));

    QVERIFY(fake.receivedRequest().contains("PUT /api/inbox/folders?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("folder")).toString(), QStringLiteral("Inbox/Old"));
    QCOMPARE(sent.value(QStringLiteral("name")).toString(), QStringLiteral("New"));
}

void RelayMailSourceTest::deleteFolderSendsDeleteWithFolderQueryParamNotBody()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"folder":"Inbox/Old","parent":"Inbox"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const DeleteFolderResult result = source.deleteFolder(serverBaseUrl, auth, QStringLiteral("Inbox/Old"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.folder, QStringLiteral("Inbox/Old"));
    QCOMPARE(result.parent, QStringLiteral("Inbox"));

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("DELETE /api/inbox/folders?"));
    QVERIFY(request.contains("folder="));
    // The folder target travels as a query param, not a JSON body.
    QVERIFY(!request.contains("Content-Length:"));
}

void RelayMailSourceTest::performActionMoveIncludesTargetMailboxInRequestBody()
{
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"action":"move","processed":2,"failed":[],"targetMailbox":"Archive"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ActionResult result = source.performAction(serverBaseUrl, auth, QStringLiteral("move"),
                                                       { QStringLiteral("m1"), QStringLiteral("m2") },
                                                       QStringLiteral("Inbox"), QStringLiteral("Archive"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.action, QStringLiteral("move"));
    QCOMPARE(result.processed, 2);
    QVERIFY(result.failed.isEmpty());
    QCOMPARE(result.targetMailbox, QStringLiteral("Archive"));

    QVERIFY(fake.receivedRequest().contains("POST /api/inbox/actions?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("action")).toString(), QStringLiteral("move"));
    QCOMPARE(sent.value(QStringLiteral("mailbox")).toString(), QStringLiteral("Inbox"));
    QCOMPARE(sent.value(QStringLiteral("messageIds")).toArray().size(), 2);
    QVERIFY(sent.contains(QStringLiteral("targetMailbox")));
    QCOMPARE(sent.value(QStringLiteral("targetMailbox")).toString(), QStringLiteral("Archive"));
}

void RelayMailSourceTest::performActionReadOmitsTargetMailboxFromRequestBodyButResponseCarriesEmptyString()
{
    // targetMailbox is ALWAYS present on the wire response (even as "" when
    // the action wasn't "move") -- but must never be sent in the *request*
    // body for a non-move action.
    FakeRelayServer fake(httpResponse(
        200, "OK",
        R"({"ok":true,"action":"read","processed":0,"failed":[{"messageId":"m3","error":"not found"}],"targetMailbox":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ActionResult result = source.performAction(serverBaseUrl, auth, QStringLiteral("read"),
                                                       { QStringLiteral("m3") }, QStringLiteral("Inbox"), std::nullopt);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.action, QStringLiteral("read"));
    QCOMPARE(result.processed, 0);
    // targetMailbox is present in the response as "", parsed as such -- not
    // an absent/default value being confused with "omitted".
    QCOMPARE(result.targetMailbox, QString());
    QVERIFY(result.targetMailbox.isEmpty());
    QCOMPARE(result.failed.size(), 1);
    QCOMPARE(result.failed.at(0).messageId, QStringLiteral("m3"));
    QCOMPARE(result.failed.at(0).error, QStringLiteral("not found"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("action")).toString(), QStringLiteral("read"));
    QVERIFY(!sent.contains(QStringLiteral("targetMailbox")));
}

void RelayMailSourceTest::sendMailJoinsRecipientsAndBase64EncodesAttachmentByteForByte()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"sentSaved":true,"warning":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    // Exercise every byte value 0-255 so the base64 round trip can't pass
    // by accident on a text-only fixture.
    QByteArray attachmentBytes;
    for (int i = 0; i < 256; ++i)
        attachmentBytes.append(static_cast<char>(i));

    MailAttachmentUpload attachment;
    attachment.name = QStringLiteral("data.bin");
    attachment.mimeType = QStringLiteral("application/octet-stream");
    attachment.data = attachmentBytes;

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const SendMailResult result =
        source.sendMail(serverBaseUrl, auth, QStringLiteral("a@example.com,b@example.com"),
                         QStringLiteral("cc@example.com"), QString(), QStringLiteral("Hello"),
                         QStringLiteral("Body text"), QStringLiteral("plain"), { attachment });

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.sentSaved, true);
    QVERIFY(result.warning.isEmpty());

    QVERIFY(fake.receivedRequest().contains("POST /api/mail/send?"));
    const QJsonObject sent = fake.receivedJsonBody();
    // to/cc/bcc travel as comma-joined strings, not JSON arrays -- this
    // client does not split/join on the caller's behalf.
    QCOMPARE(sent.value(QStringLiteral("to")).toString(), QStringLiteral("a@example.com,b@example.com"));
    QCOMPARE(sent.value(QStringLiteral("cc")).toString(), QStringLiteral("cc@example.com"));
    QCOMPARE(sent.value(QStringLiteral("bcc")).toString(), QString());
    QCOMPARE(sent.value(QStringLiteral("subject")).toString(), QStringLiteral("Hello"));
    QCOMPARE(sent.value(QStringLiteral("body")).toString(), QStringLiteral("Body text"));
    QCOMPARE(sent.value(QStringLiteral("mode")).toString(), QStringLiteral("plain"));

    const QJsonArray sentAttachments = sent.value(QStringLiteral("attachments")).toArray();
    QCOMPARE(sentAttachments.size(), 1);
    const QJsonObject sentAttachment = sentAttachments.at(0).toObject();
    QCOMPARE(sentAttachment.value(QStringLiteral("name")).toString(), QStringLiteral("data.bin"));
    QCOMPARE(sentAttachment.value(QStringLiteral("mimeType")).toString(), QStringLiteral("application/octet-stream"));

    // Byte-for-byte round trip: decode what actually reached the wire and
    // compare against the original bytes, not merely "the field is present".
    const QByteArray decoded =
        QByteArray::fromBase64(sentAttachment.value(QStringLiteral("dataBase64")).toString().toLatin1());
    QCOMPARE(decoded, attachmentBytes);
}

void RelayMailSourceTest::sendMailSendsEmptyAttachmentsArrayWhenNoneProvided()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"sentSaved":true,"warning":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    source.sendMail(serverBaseUrl, auth, QStringLiteral("a@example.com"), QString(), QString(),
                     QStringLiteral("Hi"), QStringLiteral("Body"), QStringLiteral("plain"), {});

    const QJsonObject sent = fake.receivedJsonBody();
    QVERIFY(sent.contains(QStringLiteral("attachments")));
    QVERIFY(sent.value(QStringLiteral("attachments")).toArray().isEmpty());
}

void RelayMailSourceTest::sendMailParsesAlwaysPresentWarningField()
{
    // sentSaved=false + a non-empty warning is the "sent but Sent-folder
    // save failed" case from handleMailSend -- ok is still true.
    FakeRelayServer fake(httpResponse(
        200, "OK",
        R"({"ok":true,"sentSaved":false,"warning":"email sent but could not be saved to Sent folder"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const SendMailResult result = source.sendMail(serverBaseUrl, auth, QStringLiteral("a@example.com"), QString(),
                                                    QString(), QStringLiteral("Hi"), QStringLiteral("Body"),
                                                    QStringLiteral("plain"), {});

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.sentSaved, false);
    QCOMPARE(result.warning, QStringLiteral("email sent but could not be saved to Sent folder"));
}

void RelayMailSourceTest::listAttachmentsSendsMailboxMessageIdAndAuthAsQueryParamsAndParsesResult()
{
    const QByteArray body = R"(
    {
      "ok": true,
      "attachments": [
        {"index": 0, "name": "report.pdf", "mimeType": "application/pdf", "size": 1024},
        {"index": 1, "name": "image.png", "mimeType": "image/png", "size": 2048}
      ]
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ListAttachmentsResult result =
        source.listAttachments(serverBaseUrl, auth, QStringLiteral("Inbox"), QStringLiteral("42"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.attachments.size(), 2);
    QCOMPARE(result.attachments.at(0).index, 0);
    QCOMPARE(result.attachments.at(0).name, QStringLiteral("report.pdf"));
    QCOMPARE(result.attachments.at(0).mimeType, QStringLiteral("application/pdf"));
    QCOMPARE(result.attachments.at(0).size, 1024);
    QCOMPARE(result.attachments.at(1).index, 1);
    QCOMPARE(result.attachments.at(1).name, QStringLiteral("image.png"));
    QCOMPARE(result.attachments.at(1).mimeType, QStringLiteral("image/png"));
    QCOMPARE(result.attachments.at(1).size, 2048);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachments?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
}

void RelayMailSourceTest::downloadAttachmentReturnsRawBytesAndParsesFilenameFromContentDisposition()
{
    // Every byte value 0-255, to confirm the raw body survives the round
    // trip byte-for-byte rather than being treated as/mangled like text.
    QByteArray rawBytes;
    for (int i = 0; i < 256; ++i)
        rawBytes.append(static_cast<char>(i));

    // Hand-written Content-Disposition header matching Go's
    // mime.FormatMediaType output shape exactly, including backslash-escaped
    // quotes inside the quoted filename, to exercise the escape-aware parser.
    FakeRelayServer fake(httpResponse(200, "OK", rawBytes, "application/pdf",
                                       { { "Content-Disposition", R"(attachment; filename="My File \"v2\".pdf")" } }));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const DownloadAttachmentResult result =
        source.downloadAttachment(serverBaseUrl, auth, QStringLiteral("Inbox"), QStringLiteral("42"), 0);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.data, rawBytes);
    QCOMPARE(result.mimeType, QStringLiteral("application/pdf"));
    QCOMPARE(result.filename, QStringLiteral("My File \"v2\".pdf"));

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachment?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("index=0"));
}

void RelayMailSourceTest::downloadAttachmentMapsNotFoundFrom404()
{
    FakeRelayServer fake(
        httpResponse(404, "Not Found", "attachment not found\n", "text/plain; charset=utf-8"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const DownloadAttachmentResult result =
        source.downloadAttachment(serverBaseUrl, auth, QStringLiteral("Inbox"), QStringLiteral("42"), 99);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Server);
    QVERIFY(result.data.isEmpty());
}

QTEST_GUILESS_MAIN(RelayMailSourceTest)
#include "RelayMailSourceTest.moc"
