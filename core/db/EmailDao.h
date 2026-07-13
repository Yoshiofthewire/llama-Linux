#pragma once

#include "models/Email.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

class EmailDao
{
public:
    explicit EmailDao(QSqlDatabase& db);

    bool insertOrReplace(const Email& email);
    std::optional<Email> findById(const QString& messageId) const;
    QVector<Email> findByFolder(const QString& folder) const;
    QVector<Email> findAll() const;
    bool deleteById(const QString& messageId);
    bool deleteAll();

    bool deleteByFolder(const QString& folder);

    // Wipes `folder` and inserts every email in `emails` (each with .folder
    // already set to `folder` by the caller), wrapped in one transaction so
    // a partial failure doesn't leave the folder half-replaced.
    bool replaceFolderSnapshot(const QString& folder, const QVector<Email>& emails);

private:
    QSqlDatabase& m_db;
};
