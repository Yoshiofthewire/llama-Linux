#pragma once

#include <QSettings>
#include <QString>

// Persists mail/contact sync cursors via QSettings rather than a SQLite
// table (chosen over a new schema table because a cursor is a single
// scalar-per-stream value with no query/join needs; see task 5 report).
// Cursors are stored as QString since the backend's `since`/`cursor` and
// `baseCursor` values may arrive as bare number or string on the wire.
class CursorStore
{
public:
    explicit CursorStore(const QString& filePath);

    QString mailCursor() const;
    void setMailCursor(const QString& cursor);

    QString contactBaseCursor() const;
    void setContactBaseCursor(const QString& cursor);

    // Clears both cursors back to empty/absent. Storage primitive for a
    // future ContactSyncRepository's tooOld-response reconciliation.
    void reset();

private:
    QSettings m_settings;
};
