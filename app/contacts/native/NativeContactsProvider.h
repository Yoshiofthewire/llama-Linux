#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

// One listed item from a native address book -- just enough to drive
// matching/deletion-detection (Task 6's orchestrator) without the
// orchestrator ever seeing Akonadi/EDS-shaped types. modifiedAt is optional
// because not every backend/item reliably reports a modification time (see
// NativeContactSyncRepository's conflict-tiebreak comment for what happens
// when it's absent).
struct NativeContactSummary
{
    QString nativeItemId;
    QString nativeSourceId;
    std::optional<QDateTime> modifiedAt;
};

enum class NativeContactsAvailability { Available, ServiceNotRunning, NoAddressBooksConfigured, PermissionDenied };

// Seam between NativeContactSyncRepository (app/contacts/native/, this
// task) and a real backend (Akonadi/EDS, Tasks 7-8, not yet implemented).
// Deliberately header-only/pure-virtual and living in app/ rather than
// core/ -- the eventual real implementations need QtDBus/Akonadi types that
// core/ must never depend on, even though this interface itself doesn't
// need them yet.
class NativeContactsProvider
{
public:
    virtual ~NativeContactsProvider() = default;

    virtual QString backendId() const = 0;
    virtual NativeContactsAvailability probeAvailability() = 0;
    virtual QVector<NativeContactSummary> listItems() = 0;
    virtual std::optional<QString> fetchVCard(const QString& nativeItemId) = 0;
    virtual std::optional<QString> createItem(const QString& nativeSourceId, const QString& vCard) = 0;
    virtual bool updateItem(const QString& nativeItemId, const QString& vCard) = 0;
    virtual bool deleteItem(const QString& nativeItemId) = 0;
    virtual std::optional<QString> defaultSourceId() = 0;
};
