#include "stores/CursorStore.h"

namespace {
constexpr auto kMailCursorKey = "sync/mailCursor";
constexpr auto kContactBaseCursorKey = "sync/contactBaseCursor";
} // namespace

CursorStore::CursorStore(const QString& filePath)
    : m_settings(filePath, QSettings::IniFormat)
{
}

QString CursorStore::mailCursor() const
{
    return m_settings.value(kMailCursorKey, QString()).toString();
}

void CursorStore::setMailCursor(const QString& cursor)
{
    m_settings.setValue(kMailCursorKey, cursor);
}

QString CursorStore::contactBaseCursor() const
{
    return m_settings.value(kContactBaseCursorKey, QString()).toString();
}

void CursorStore::setContactBaseCursor(const QString& cursor)
{
    m_settings.setValue(kContactBaseCursorKey, cursor);
}

void CursorStore::reset()
{
    m_settings.remove(kMailCursorKey);
    m_settings.remove(kContactBaseCursorKey);
}
