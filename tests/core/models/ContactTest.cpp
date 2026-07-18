#include "models/Contact.h"

#include <QTest>

class ContactTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructs();
    void populatesAndCompares();
    void entryTypesCompare();
};

void ContactTest::defaultConstructs()
{
    Contact contact;
    QVERIFY(contact.uid.isEmpty());
    QCOMPARE(contact.rev, qint64(0));
    QVERIFY(!contact.createdAt.has_value());
    QVERIFY(!contact.updatedAt.has_value());
    QVERIFY(!contact.fn.has_value());
    QVERIFY(!contact.givenName.has_value());
    QVERIFY(!contact.familyName.has_value());
    QVERIFY(!contact.middleName.has_value());
    QVERIFY(!contact.prefix.has_value());
    QVERIFY(!contact.suffix.has_value());
    QVERIFY(!contact.nickname.has_value());
    QVERIFY(!contact.org.has_value());
    QVERIFY(!contact.title.has_value());
    QVERIFY(!contact.notes.has_value());
    QVERIFY(!contact.birthday.has_value());
    QVERIFY(contact.emails.isEmpty());
    QVERIFY(contact.phones.isEmpty());
    QVERIFY(contact.addresses.isEmpty());
    QVERIFY(contact.groupIds.isEmpty());
    QVERIFY(!contact.photoRef.has_value());
    QVERIFY(!contact.pgpKey.has_value());
    QVERIFY(contact.ims.isEmpty());
    QVERIFY(contact.websites.isEmpty());
    QVERIFY(contact.relations.isEmpty());
    QVERIFY(contact.events.isEmpty());
    QVERIFY(!contact.phoneticGivenName.has_value());
    QVERIFY(!contact.phoneticFamilyName.has_value());
    QVERIFY(!contact.department.has_value());
    QVERIFY(contact.customFields.isEmpty());
    QVERIFY(!contact.pronouns.has_value());
    QCOMPARE(contact.isSelf, false);
    QVERIFY(contact.mergedUIDs.isEmpty());
    QVERIFY(!contact.mergedInto.has_value());
}

void ContactTest::populatesAndCompares()
{
    Contact contact;
    contact.uid = QStringLiteral("uid-1");
    contact.rev = 42;
    contact.createdAt = QStringLiteral("2026-01-01T00:00:00Z");
    contact.updatedAt = QStringLiteral("2026-02-01T00:00:00Z");
    contact.fn = QStringLiteral("Ada Lovelace");
    contact.givenName = QStringLiteral("Ada");
    contact.familyName = QStringLiteral("Lovelace");
    contact.middleName = QStringLiteral("Augusta");
    contact.prefix = QStringLiteral("Countess");
    contact.suffix = QStringLiteral("Esq.");
    contact.nickname = QStringLiteral("Ada");
    contact.org = QStringLiteral("Analytical Engines Ltd");
    contact.title = QStringLiteral("Mathematician");
    contact.notes = QStringLiteral("Notes");
    contact.birthday = QStringLiteral("1815-12-10");
    contact.emails = {ContactEmailEntry{QStringLiteral("work"), QStringLiteral("ada@example.com")}};
    contact.phones = {ContactPhoneEntry{QStringLiteral("mobile"), QStringLiteral("+1-555-0100")}};
    contact.addresses = {ContactAddressEntry{
        QStringLiteral("home"), QStringLiteral("1 Main St"), QStringLiteral("London"),
        QStringLiteral("London"), QStringLiteral("SW1A 1AA"), QStringLiteral("UK")}};
    contact.groupIds = {QStringLiteral("group-1"), QStringLiteral("group-2")};
    contact.photoRef = QStringLiteral("photo-ref-1");
    contact.pgpKey = QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----");
    contact.ims = {ContactImEntry{QStringLiteral("Matrix"), QStringLiteral("work"), QStringLiteral("@ada:example.org")}};
    contact.websites = {ContactUrlEntry{QStringLiteral("blog"), QStringLiteral("https://ada.example.com")}};
    contact.relations = {ContactRelationEntry{QStringLiteral("spouse"), QStringLiteral("William King")}};
    contact.events = {ContactEventEntry{QStringLiteral("anniversary"), QStringLiteral("2026-06-01")}};
    contact.phoneticGivenName = QStringLiteral("Ay-da");
    contact.phoneticFamilyName = QStringLiteral("Love-lace");
    contact.department = QStringLiteral("Engineering");
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");
    contact.isSelf = true;
    contact.mergedUIDs = {QStringLiteral("merged-1"), QStringLiteral("merged-2")};
    contact.mergedInto = QStringLiteral("survivor-uid");

    Contact copy = contact;
    QCOMPARE(copy, contact);

    Contact assigned;
    assigned = contact;
    QCOMPARE(assigned, contact);

    Contact different = contact;
    different.fn = QStringLiteral("Different Name");
    QVERIFY(different != contact);
}

void ContactTest::entryTypesCompare()
{
    ContactEmailEntry email1{QStringLiteral("work"), QStringLiteral("a@example.com")};
    ContactEmailEntry email2{QStringLiteral("work"), QStringLiteral("a@example.com")};
    ContactEmailEntry email3{std::nullopt, QStringLiteral("a@example.com")};
    QCOMPARE(email1, email2);
    QVERIFY(email1 != email3);

    ContactPhoneEntry phone1{QStringLiteral("mobile"), QStringLiteral("123")};
    ContactPhoneEntry phone2{QStringLiteral("mobile"), QStringLiteral("123")};
    QCOMPARE(phone1, phone2);

    ContactAddressEntry addr1{QStringLiteral("home"), QStringLiteral("St"), std::nullopt,
                               std::nullopt, std::nullopt, std::nullopt};
    ContactAddressEntry addr2 = addr1;
    QCOMPARE(addr1, addr2);
    addr2.street = QStringLiteral("Other St");
    QVERIFY(addr1 != addr2);

    ContactImEntry im1{QStringLiteral("Matrix"), QStringLiteral("work"), QStringLiteral("@ada:example.org")};
    ContactImEntry im2 = im1;
    QCOMPARE(im1, im2);
    im2.service = std::nullopt;
    QVERIFY(im1 != im2);

    ContactUrlEntry url1{QStringLiteral("blog"), QStringLiteral("https://ada.example.com")};
    ContactUrlEntry url2 = url1;
    QCOMPARE(url1, url2);
    url2.value = QStringLiteral("https://other.example.com");
    QVERIFY(url1 != url2);

    ContactRelationEntry rel1{QStringLiteral("spouse"), QStringLiteral("William King")};
    ContactRelationEntry rel2 = rel1;
    QCOMPARE(rel1, rel2);
    rel2.name = QStringLiteral("Someone Else");
    QVERIFY(rel1 != rel2);

    ContactEventEntry event1{QStringLiteral("anniversary"), QStringLiteral("2026-06-01")};
    ContactEventEntry event2 = event1;
    QCOMPARE(event1, event2);
    event2.date = QStringLiteral("2027-06-01");
    QVERIFY(event1 != event2);

    ContactCustomFieldEntry custom1{QStringLiteral("Employee ID"), QStringLiteral("42")};
    ContactCustomFieldEntry custom2 = custom1;
    QCOMPARE(custom1, custom2);
    custom2.value = QStringLiteral("43");
    QVERIFY(custom1 != custom2);
}

QTEST_APPLESS_MAIN(ContactTest)
#include "ContactTest.moc"
