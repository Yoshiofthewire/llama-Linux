#include "contacts/ContactListModel.h"

ContactListModel::ContactListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ContactListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_contacts.size();
}

QVariant ContactListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contacts.size())
        return QVariant();

    const Contact& contact = m_contacts.at(index.row());
    switch (role) {
    case UidRole:
        return contact.uid;
    case RevRole:
        return contact.rev;
    case FnRole:
        return contact.fn.value_or(QString());
    case GivenNameRole:
        return contact.givenName.value_or(QString());
    case FamilyNameRole:
        return contact.familyName.value_or(QString());
    case OrgRole:
        return contact.org.value_or(QString());
    case NotesRole:
        return contact.notes.value_or(QString());
    case PrimaryEmailRole:
        return contact.emails.isEmpty() ? QString() : contact.emails.first().value;
    case PrimaryPhoneRole:
        return contact.phones.isEmpty() ? QString() : contact.phones.first().value;
    case SyncedRole:
        return !m_pendingUids.contains(contact.uid); // see the class doc comment in ContactListModel.h
    case PhotoRefRole:
        return contact.photoRef.value_or(QString());
    case IsSelfRole:
        return contact.isSelf;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ContactListModel::roleNames() const
{
    return {
        { UidRole, "uid" },
        { RevRole, "rev" },
        { FnRole, "fn" },
        { GivenNameRole, "givenName" },
        { FamilyNameRole, "familyName" },
        { OrgRole, "org" },
        { NotesRole, "notes" },
        { PrimaryEmailRole, "primaryEmail" },
        { PrimaryPhoneRole, "primaryPhone" },
        { SyncedRole, "synced" },
        { PhotoRefRole, "photoRef" },
        { IsSelfRole, "isSelf" },
    };
}

void ContactListModel::setContacts(const QVector<Contact>& contacts, const QSet<QString>& pendingUids)
{
    beginResetModel();
    m_contacts = contacts;
    m_pendingUids = pendingUids;
    endResetModel();
}

Contact ContactListModel::contactAt(int row) const
{
    if (row < 0 || row >= m_contacts.size())
        return Contact();
    return m_contacts.at(row);
}
