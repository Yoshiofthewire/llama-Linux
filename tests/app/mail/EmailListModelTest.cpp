#include "mail/EmailListModel.h"

#include "models/Email.h"

#include <QTest>

class EmailListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void rowCountAndEmailAtReflectSetEmails();
    void dataRoundTripsEveryRoleForAPopulatedRow();
    void emailAtOutOfRangeReturnsDefaultConstructedEmail();
    void dataOutOfRangeReturnsInvalidVariant();

private:
    static Email sampleEmail();
};

Email EmailListModelTest::sampleEmail()
{
    Email email;
    email.messageId = QStringLiteral("m1");
    email.folder = QStringLiteral("INBOX");
    email.sender = QStringLiteral("alice@example.com");
    email.sentTo = QStringLiteral("bob@example.com");
    email.cc = QStringLiteral("cc@example.com");
    email.bcc = QStringLiteral("bcc@example.com");
    email.subject = QStringLiteral("Hello");
    email.preview = QStringLiteral("Preview text");
    email.body = QStringLiteral("Body text");
    email.label = QStringLiteral("important");
    email.keywords = { QStringLiteral("Work"), QStringLiteral("Urgent") };
    email.status = QStringLiteral("unread");
    email.atUtc = QStringLiteral("2026-07-01T12:00:00Z");
    email.hasAttachments = true;
    email.sourceMode = QStringLiteral("plain");
    return email;
}

void EmailListModelTest::rowCountAndEmailAtReflectSetEmails()
{
    EmailListModel model;
    QCOMPARE(model.rowCount(), 0);

    const QVector<Email> emails = { sampleEmail(), sampleEmail() };
    model.setEmails(emails);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.emailAt(0).messageId, QStringLiteral("m1"));
    QCOMPARE(model.emailAt(1).messageId, QStringLiteral("m1"));
}

void EmailListModelTest::dataRoundTripsEveryRoleForAPopulatedRow()
{
    EmailListModel model;
    model.setEmails({ sampleEmail() });

    const QModelIndex index = model.index(0, 0);
    QVERIFY(index.isValid());

    QCOMPARE(model.data(index, EmailListModel::MessageIdRole).toString(), QStringLiteral("m1"));
    QCOMPARE(model.data(index, EmailListModel::FolderRole).toString(), QStringLiteral("INBOX"));
    QCOMPARE(model.data(index, EmailListModel::SenderRole).toString(), QStringLiteral("alice@example.com"));
    QCOMPARE(model.data(index, EmailListModel::SentToRole).toString(), QStringLiteral("bob@example.com"));
    QCOMPARE(model.data(index, EmailListModel::CcRole).toString(), QStringLiteral("cc@example.com"));
    QCOMPARE(model.data(index, EmailListModel::BccRole).toString(), QStringLiteral("bcc@example.com"));
    QCOMPARE(model.data(index, EmailListModel::SubjectRole).toString(), QStringLiteral("Hello"));
    QCOMPARE(model.data(index, EmailListModel::PreviewRole).toString(), QStringLiteral("Preview text"));
    QCOMPARE(model.data(index, EmailListModel::BodyRole).toString(), QStringLiteral("Body text"));
    QCOMPARE(model.data(index, EmailListModel::LabelRole).toString(), QStringLiteral("important"));
    QCOMPARE(model.data(index, EmailListModel::KeywordsRole).toStringList(),
             QStringList({ QStringLiteral("Work"), QStringLiteral("Urgent") }));
    QCOMPARE(model.data(index, EmailListModel::StatusRole).toString(), QStringLiteral("unread"));
    QCOMPARE(model.data(index, EmailListModel::AtUtcRole).toString(), QStringLiteral("2026-07-01T12:00:00Z"));
    QCOMPARE(model.data(index, EmailListModel::HasAttachmentsRole).toBool(), true);
    QCOMPARE(model.data(index, EmailListModel::SourceModeRole).toString(), QStringLiteral("plain"));

    // roleNames() must expose exactly these 15 role-name strings for QML.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.size(), 15);
    QCOMPARE(roles.value(EmailListModel::MessageIdRole), QByteArrayLiteral("messageId"));
    QCOMPARE(roles.value(EmailListModel::KeywordsRole), QByteArrayLiteral("keywords"));
    QCOMPARE(roles.value(EmailListModel::HasAttachmentsRole), QByteArrayLiteral("hasAttachments"));
    QCOMPARE(roles.value(EmailListModel::SourceModeRole), QByteArrayLiteral("sourceMode"));
}

void EmailListModelTest::emailAtOutOfRangeReturnsDefaultConstructedEmail()
{
    EmailListModel model;
    model.setEmails({ sampleEmail() });

    QCOMPARE(model.emailAt(-1), Email());
    QCOMPARE(model.emailAt(1), Email());
}

void EmailListModelTest::dataOutOfRangeReturnsInvalidVariant()
{
    EmailListModel model;
    model.setEmails({ sampleEmail() });

    QVERIFY(!model.data(model.index(1, 0), EmailListModel::MessageIdRole).isValid());
    QVERIFY(!model.data(QModelIndex(), EmailListModel::MessageIdRole).isValid());
}

QTEST_GUILESS_MAIN(EmailListModelTest)
#include "EmailListModelTest.moc"
