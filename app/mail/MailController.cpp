#include "mail/MailController.h"

#include "domain/DevicePairing.h"
#include "domain/KeywordRepository.h"
#include "domain/MailRepository.h"
#include "domain/PairingStore.h"
#include "models/StandardFolder.h"
#include "net/RelayAuth.h"
#include "net/RelayMailSource.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>
#include <QSet>
#include <algorithm>

MailController::MailController(MailRepository& mailRepository, RelayMailSource& relayMailSource,
                                KeywordRepository& keywordRepository, PairingStore& pairingStore, QObject* parent)
    : QObject(parent)
    , m_mailRepository(mailRepository)
    , m_relayMailSource(relayMailSource)
    , m_keywordRepository(keywordRepository)
    , m_pairingStore(pairingStore)
    , m_model(new EmailListModel(this))
{
}

QObject* MailController::emailModel() const
{
    return m_model;
}

QString MailController::currentFolder() const
{
    return m_currentFolder;
}

QString MailController::selectedKeyword() const
{
    return m_selectedKeyword;
}

QVariantList MailController::keywordTabs() const
{
    const QVector<KeywordTab> tabs = m_keywordRepository.visibleTabs(m_currentFolderEmails);
    QVariantList list;
    list.reserve(tabs.size());
    for (const KeywordTab& tab : tabs) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = tab.name;
        entry[QStringLiteral("count")] = tab.count;
        list.append(entry);
    }
    return list;
}

bool MailController::isBusy() const
{
    return m_isBusy;
}

QString MailController::lastError() const
{
    return m_lastError;
}

void MailController::setBusy(bool busy)
{
    if (m_isBusy == busy)
        return;
    m_isBusy = busy;
    emit isBusyChanged();
}

void MailController::setLastError(const QString& error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void MailController::applyFilter()
{
    if (m_selectedKeyword.isEmpty()) {
        m_model->setEmails(m_currentFolderEmails);
        return;
    }
    QVector<Email> filtered;
    for (const Email& email : m_currentFolderEmails) {
        if (email.keywords.contains(m_selectedKeyword))
            filtered.append(email);
    }
    m_model->setEmails(filtered);
}

bool MailController::requirePairing(QUrl& serverBaseUrl, RelayAuth& auth)
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing) {
        setLastError(QStringLiteral("Not paired"));
        return false;
    }
    serverBaseUrl = QUrl(pairing->serverBaseUrl);
    auth = RelayAuth{ pairing->subscriberId, pairing->subscriberHash };
    return true;
}

void MailController::selectFolder(const QString& wireFolder)
{
    if (m_currentFolder != wireFolder) {
        m_currentFolder = wireFolder;
        emit currentFolderChanged();
    }
    if (!m_selectedKeyword.isEmpty()) {
        m_selectedKeyword.clear();
        emit selectedKeywordChanged();
    }
    m_currentFolderEmails = m_mailRepository.cachedEmails(m_currentFolder);
    emit keywordTabsChanged();
    applyFilter();
    refresh();
}

void MailController::selectKeyword(const QString& keyword)
{
    if (m_selectedKeyword == keyword)
        return;
    m_selectedKeyword = keyword;
    emit selectedKeywordChanged();
    applyFilter();
}

void MailController::refresh(bool forceFullResync)
{
    setBusy(true);
    const MailFetchOutcome outcome = m_mailRepository.refreshFolder(m_currentFolder, forceFullResync);
    setBusy(false);

    if (outcome.outcome != MailRepositoryOutcome::Success)
        setLastError(outcome.detail.isEmpty() ? QStringLiteral("Refresh failed") : outcome.detail);
    else
        setLastError(QString());

    m_currentFolderEmails = m_mailRepository.cachedEmails(m_currentFolder);
    emit keywordTabsChanged();
    applyFilter();
}

bool MailController::performActionCommon(const QStringList& messageIds, const QString& action,
                                          const std::optional<QString>& targetMailbox)
{
    QUrl serverBaseUrl;
    RelayAuth auth;
    if (!requirePairing(serverBaseUrl, auth))
        return false;

    setBusy(true);
    const ActionResult result =
        m_relayMailSource.performAction(serverBaseUrl, auth, action, messageIds, m_currentFolder, targetMailbox);
    setBusy(false);

    if (result.error.has_value() || !result.ok) {
        setLastError(result.detail.isEmpty() ? QStringLiteral("Action failed") : result.detail);
        return false;
    }

    // Optimistic local update (mirrors Android's InboxActivity/
    // EmailDetailActivity swipe-action pattern): drop every messageId the
    // server actually processed from the cached folder emails and re-apply
    // the filter immediately, rather than forcing a full refresh() after
    // every action. Per-message failures reported in result.failed are left
    // in place locally (the server did not act on them) and surfaced via
    // lastError, but the call itself still counts as a success since the
    // server accepted and partially processed the request (ActionResult::ok).
    QSet<QString> failedIds;
    for (const ActionFailure& failure : result.failed)
        failedIds.insert(failure.messageId);

    m_currentFolderEmails.erase(std::remove_if(m_currentFolderEmails.begin(), m_currentFolderEmails.end(),
                                                [&](const Email& email) {
                                                    return messageIds.contains(email.messageId)
                                                        && !failedIds.contains(email.messageId);
                                                }),
                                 m_currentFolderEmails.end());
    emit keywordTabsChanged();
    applyFilter();

    if (!result.failed.isEmpty()) {
        QStringList details;
        for (const ActionFailure& failure : result.failed)
            details << failure.messageId + QStringLiteral(": ") + failure.error;
        setLastError(details.join(QStringLiteral("; ")));
    } else {
        setLastError(QString());
    }
    return true;
}

bool MailController::archiveEmails(const QStringList& messageIds)
{
    return performActionCommon(messageIds, QStringLiteral("archive"), std::nullopt);
}

bool MailController::deleteEmails(const QStringList& messageIds)
{
    return performActionCommon(messageIds, QStringLiteral("delete"), std::nullopt);
}

bool MailController::markSpam(const QStringList& messageIds)
{
    return performActionCommon(messageIds, QStringLiteral("spam"), std::nullopt);
}

bool MailController::moveEmails(const QStringList& messageIds, const QString& targetFolder)
{
    return performActionCommon(messageIds, QStringLiteral("move"), targetFolder);
}

bool MailController::sendMail(const QString& to, const QString& cc, const QString& bcc, const QString& subject,
                               const QString& body, const QStringList& attachmentFilePaths)
{
    QUrl serverBaseUrl;
    RelayAuth auth;
    if (!requirePairing(serverBaseUrl, auth))
        return false;

    // Matches Android's MAX_ATTACHMENT_BYTES / the backend's own cap.
    static constexpr qint64 kMaxAttachmentBytes = 25LL * 1024 * 1024;

    QMimeDatabase mimeDb;
    QVector<MailAttachmentUpload> attachments;
    attachments.reserve(attachmentFilePaths.size());
    qint64 totalBytes = 0;
    for (const QString& path : attachmentFilePaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            setLastError(QStringLiteral("Could not open attachment: %1").arg(path));
            return false;
        }
        const QByteArray data = file.readAll();
        totalBytes += data.size();
        if (totalBytes > kMaxAttachmentBytes) {
            setLastError(QStringLiteral("Attachments exceed the 25 MB limit"));
            return false;
        }
        MailAttachmentUpload upload;
        upload.name = QFileInfo(path).fileName();
        upload.mimeType = mimeDb.mimeTypeForFile(path).name();
        upload.data = data;
        attachments.append(upload);
    }

    setBusy(true);
    const SendMailResult result = m_relayMailSource.sendMail(serverBaseUrl, auth, to, cc, bcc, subject, body,
                                                               QStringLiteral("plain"), attachments);
    setBusy(false);

    if (result.error.has_value() || !result.ok) {
        setLastError(result.detail.isEmpty() ? QStringLiteral("Send failed") : result.detail);
        return false;
    }
    setLastError(QString());
    return true;
}

QVariantList MailController::listAttachments(const QString& mailbox, const QString& messageId)
{
    QUrl serverBaseUrl;
    RelayAuth auth;
    if (!requirePairing(serverBaseUrl, auth))
        return {};

    setBusy(true);
    const ListAttachmentsResult result = m_relayMailSource.listAttachments(serverBaseUrl, auth, mailbox, messageId);
    setBusy(false);

    if (result.error.has_value()) {
        setLastError(result.detail.isEmpty() ? QStringLiteral("Could not list attachments") : result.detail);
        return {};
    }
    setLastError(QString());

    QVariantList list;
    list.reserve(result.attachments.size());
    for (const MailAttachmentInfo& info : result.attachments) {
        QVariantMap entry;
        entry[QStringLiteral("index")] = info.index;
        entry[QStringLiteral("name")] = info.name;
        entry[QStringLiteral("mimeType")] = info.mimeType;
        entry[QStringLiteral("size")] = info.size;
        list.append(entry);
    }
    return list;
}

QString MailController::dedupedFilePath(const QString& directory, const QString& fileName)
{
    const QFileInfo info(fileName);
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = fileName;
    int suffixCounter = 1;
    while (QFile::exists(directory + QStringLiteral("/") + candidate)) {
        candidate = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(suffixCounter)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(suffixCounter).arg(suffix);
        ++suffixCounter;
    }
    return directory + QStringLiteral("/") + candidate;
}

bool MailController::downloadAttachment(const QString& mailbox, const QString& messageId, int index,
                                         const QString& suggestedName)
{
    QUrl serverBaseUrl;
    RelayAuth auth;
    if (!requirePairing(serverBaseUrl, auth))
        return false;

    setBusy(true);
    const DownloadAttachmentResult result =
        m_relayMailSource.downloadAttachment(serverBaseUrl, auth, mailbox, messageId, index);
    setBusy(false);

    if (result.error.has_value()) {
        setLastError(result.detail.isEmpty() ? QStringLiteral("Download failed") : result.detail);
        return false;
    }

    // Prefer the caller-supplied name (QML typically already has the
    // attachment's display name from listAttachments()); fall back to
    // whatever filename the download response's Content-Disposition header
    // carried, and finally to a generic name if both are somehow empty.
    QString name = suggestedName.isEmpty() ? result.filename : suggestedName;
    if (name.isEmpty())
        name = QStringLiteral("attachment");

    const QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(downloadDir);
    const QString targetPath = dedupedFilePath(downloadDir, name);

    QFile outFile(targetPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        setLastError(QStringLiteral("Could not write attachment to %1").arg(targetPath));
        return false;
    }
    outFile.write(result.data);
    outFile.close();

    setLastError(QString());
    return true;
}

QVariantList MailController::standardFolders() const
{
    static constexpr StandardFolder kFolders[] = {
        StandardFolder::Inbox, StandardFolder::Drafts, StandardFolder::Junk,
        StandardFolder::Sent,  StandardFolder::Trash,  StandardFolder::Archive,
    };

    QVariantList list;
    list.reserve(6);
    for (StandardFolder folder : kFolders) {
        const QString wireName = standardFolderWireName(folder);
        QVariantMap entry;
        entry[QStringLiteral("wireName")] = wireName;
        entry[QStringLiteral("displayName")] = standardFolderDisplayName(wireName);
        list.append(entry);
    }
    return list;
}
