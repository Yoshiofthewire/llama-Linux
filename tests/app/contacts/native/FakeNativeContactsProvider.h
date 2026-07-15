#pragma once

// In-memory double for NativeContactsProvider -- there is no real Akonadi/
// EDS daemon to fake at the transport level in this session (unlike
// tests/core/net/FakeRelayServer.h's real QTcpServer), so this is just a
// QHash of itemId->vCard text plus settable availability/per-item
// modifiedAt/sourceId, letting tests script exactly the presence/absence/
// edit scenario they need with no hidden behavior.

#include "contacts/native/NativeContactsProvider.h"

#include <QHash>
#include <QString>

class FakeNativeContactsProvider : public NativeContactsProvider
{
public:
    QString backendId() const override { return m_backendId; }
    NativeContactsAvailability probeAvailability() override { return m_availability; }

    QVector<NativeContactSummary> listItems() override
    {
        QVector<NativeContactSummary> result;
        for (auto it = m_vCards.constBegin(); it != m_vCards.constEnd(); ++it) {
            NativeContactSummary summary;
            summary.nativeItemId = it.key();
            summary.nativeSourceId = m_sourceIds.value(it.key());
            summary.modifiedAt = m_modifiedAt.value(it.key(), std::nullopt);
            result.append(summary);
        }
        return result;
    }

    std::optional<QString> fetchVCard(const QString& nativeItemId) override
    {
        auto it = m_vCards.constFind(nativeItemId);
        if (it == m_vCards.constEnd())
            return std::nullopt;
        return it.value();
    }

    std::optional<QString> createItem(const QString& nativeSourceId, const QString& vCard) override
    {
        const QString itemId = QStringLiteral("native-created-%1").arg(m_nextItemId++);
        m_vCards.insert(itemId, vCard);
        m_sourceIds.insert(itemId, nativeSourceId);
        return itemId;
    }

    bool updateItem(const QString& nativeItemId, const QString& vCard) override
    {
        if (!m_vCards.contains(nativeItemId))
            return false;
        m_vCards.insert(nativeItemId, vCard);
        return true;
    }

    bool deleteItem(const QString& nativeItemId) override { return m_vCards.remove(nativeItemId) > 0; }

    std::optional<QString> defaultSourceId() override { return m_defaultSourceId; }

    // --- test scripting helpers, not part of the interface ---

    void setBackendId(const QString& backendId) { m_backendId = backendId; }
    void setAvailability(NativeContactsAvailability availability) { m_availability = availability; }
    void setDefaultSourceId(std::optional<QString> sourceId) { m_defaultSourceId = std::move(sourceId); }

    // Seeds an existing native item (as if it were already present before
    // sync() ever ran), returning its assigned itemId.
    QString seedItem(const QString& vCard, const QString& sourceId = QStringLiteral("source-1"),
                      std::optional<QDateTime> modifiedAt = std::nullopt)
    {
        const QString itemId = QStringLiteral("native-seed-%1").arg(m_nextItemId++);
        m_vCards.insert(itemId, vCard);
        m_sourceIds.insert(itemId, sourceId);
        m_modifiedAt.insert(itemId, modifiedAt);
        return itemId;
    }

    void setModifiedAt(const QString& itemId, std::optional<QDateTime> modifiedAt)
    {
        m_modifiedAt.insert(itemId, modifiedAt);
    }

    bool hasItem(const QString& itemId) const { return m_vCards.contains(itemId); }
    QString vCardFor(const QString& itemId) const { return m_vCards.value(itemId); }
    int itemCount() const { return m_vCards.size(); }

private:
    QString m_backendId = QStringLiteral("fake");
    NativeContactsAvailability m_availability = NativeContactsAvailability::Available;
    std::optional<QString> m_defaultSourceId = QStringLiteral("source-1");
    QHash<QString, QString> m_vCards;
    QHash<QString, QString> m_sourceIds;
    QHash<QString, std::optional<QDateTime>> m_modifiedAt;
    int m_nextItemId = 1;
};
