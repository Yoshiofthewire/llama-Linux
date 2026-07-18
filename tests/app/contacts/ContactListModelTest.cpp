#include "contacts/ContactListModel.h"

#include "models/Contact.h"

#include <QTest>

class ContactListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void rowCountAndContactAtReflectSetContacts();
    void dataRoundTripsEveryRoleForAPopulatedRow();
    void syncedRoleReflectsPendingUidSet();
    void contactAtOutOfRangeReturnsDefaultConstructedContact();
    void dataOutOfRangeReturnsInvalidVariant();

private:
    static Contact sampleContact();
};

Contact ContactListModelTest::sampleContact()
{
    Contact contact;
    contact.uid = QStringLiteral("srv-1");
    contact.rev = 3;
    contact.fn = QStringLiteral("Ada Lovelace");
    contact.givenName = QStringLiteral("Ada");
    contact.familyName = QStringLiteral("Lovelace");
    contact.org = QStringLiteral("Analytical Engines Inc");
    contact.notes = QStringLiteral("Loves math");
    contact.emails = { ContactEmailEntry{ std::nullopt, QStringLiteral("ada@example.com") },
                        ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("ada.work@example.com") } };
    contact.phones = { ContactPhoneEntry{ std::nullopt, QStringLiteral("555-1234") } };
    contact.photoRef = QStringLiteral("photo-ref-1");
    return contact;
}

void ContactListModelTest::rowCountAndContactAtReflectSetContacts()
{
    ContactListModel model;
    QCOMPARE(model.rowCount(), 0);

    const QVector<Contact> contacts = { sampleContact(), sampleContact() };
    model.setContacts(contacts);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.contactAt(0).uid, QStringLiteral("srv-1"));
    QCOMPARE(model.contactAt(1).uid, QStringLiteral("srv-1"));
}

void ContactListModelTest::dataRoundTripsEveryRoleForAPopulatedRow()
{
    ContactListModel model;
    model.setContacts({ sampleContact() });

    const QModelIndex index = model.index(0, 0);
    QVERIFY(index.isValid());

    QCOMPARE(model.data(index, ContactListModel::UidRole).toString(), QStringLiteral("srv-1"));
    QCOMPARE(model.data(index, ContactListModel::RevRole).toLongLong(), qint64(3));
    QCOMPARE(model.data(index, ContactListModel::FnRole).toString(), QStringLiteral("Ada Lovelace"));
    QCOMPARE(model.data(index, ContactListModel::GivenNameRole).toString(), QStringLiteral("Ada"));
    QCOMPARE(model.data(index, ContactListModel::FamilyNameRole).toString(), QStringLiteral("Lovelace"));
    QCOMPARE(model.data(index, ContactListModel::OrgRole).toString(), QStringLiteral("Analytical Engines Inc"));
    QCOMPARE(model.data(index, ContactListModel::NotesRole).toString(), QStringLiteral("Loves math"));
    // primaryEmail/primaryPhone are derived from index 0 of emails/phones.
    QCOMPARE(model.data(index, ContactListModel::PrimaryEmailRole).toString(), QStringLiteral("ada@example.com"));
    QCOMPARE(model.data(index, ContactListModel::PrimaryPhoneRole).toString(), QStringLiteral("555-1234"));
    QCOMPARE(model.data(index, ContactListModel::SyncedRole).toBool(), true); // rev == 3, not 0
    QCOMPARE(model.data(index, ContactListModel::PhotoRefRole).toString(), QStringLiteral("photo-ref-1"));

    // roleNames() must expose exactly these 11 role-name strings for QML.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.size(), 11);
    QCOMPARE(roles.value(ContactListModel::UidRole), QByteArrayLiteral("uid"));
    QCOMPARE(roles.value(ContactListModel::PrimaryEmailRole), QByteArrayLiteral("primaryEmail"));
    QCOMPARE(roles.value(ContactListModel::PrimaryPhoneRole), QByteArrayLiteral("primaryPhone"));
    QCOMPARE(roles.value(ContactListModel::SyncedRole), QByteArrayLiteral("synced"));
    QCOMPARE(roles.value(ContactListModel::PhotoRefRole), QByteArrayLiteral("photoRef"));
}

void ContactListModelTest::syncedRoleReflectsPendingUidSet()
{
    // synced == !pendingUids.contains(uid) (see ContactListModel.h's doc
    // comment) -- membership in the pending-uid set supplied alongside
    // setContacts() is the real ground truth, not any field on Contact
    // itself. rev is set identically on both rows here to prove the role
    // no longer derives from rev at all.
    Contact unsynced;
    unsynced.uid = QStringLiteral("temp-local-uid");
    unsynced.rev = 1;
    unsynced.fn = QStringLiteral("Not Yet Synced");

    Contact synced = sampleContact();
    synced.rev = 1;

    ContactListModel model;
    model.setContacts({ unsynced, synced }, { QStringLiteral("temp-local-uid") });

    QCOMPARE(model.data(model.index(0, 0), ContactListModel::SyncedRole).toBool(), false);
    QCOMPARE(model.data(model.index(1, 0), ContactListModel::SyncedRole).toBool(), true);
}

void ContactListModelTest::contactAtOutOfRangeReturnsDefaultConstructedContact()
{
    ContactListModel model;
    model.setContacts({ sampleContact() });

    QCOMPARE(model.contactAt(-1), Contact());
    QCOMPARE(model.contactAt(1), Contact());
}

void ContactListModelTest::dataOutOfRangeReturnsInvalidVariant()
{
    ContactListModel model;
    model.setContacts({ sampleContact() });

    QVERIFY(!model.data(model.index(1, 0), ContactListModel::UidRole).isValid());
    QVERIFY(!model.data(QModelIndex(), ContactListModel::UidRole).isValid());
}

QTEST_GUILESS_MAIN(ContactListModelTest)
#include "ContactListModelTest.moc"
