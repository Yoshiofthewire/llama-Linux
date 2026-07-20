#include "Database.h"

#include "MigrationSql.h"

#include <QAtomicInteger>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

QAtomicInteger<quint64> g_connectionCounter{0};

} // namespace

Database::Database() = default;

Database::~Database()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    if (!m_connectionName.isEmpty())
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open(const QString& path)
{
    m_connectionName = QStringLiteral("llama_db_%1").arg(g_connectionCounter.fetchAndAddRelaxed(1));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open())
        return false;

    QSqlQuery foreignKeysQuery(m_db);
    foreignKeysQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next())
        return false;
    const int version = versionQuery.value(0).toInt();

    for (int nextVersion = version + 1; nextVersion <= kKyPostMigrationCount; ++nextVersion) {
        const QString sql = kKyPostMigrationSql[nextVersion - 1]();
        const QStringList statements = sql.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& rawStatement : statements) {
            const QString statement = rawStatement.trimmed();
            if (statement.isEmpty())
                continue;
            QSqlQuery schemaQuery(m_db);
            if (!schemaQuery.exec(statement))
                return false;
        }
        QSqlQuery setVersionQuery(m_db);
        if (!setVersionQuery.exec(QStringLiteral("PRAGMA user_version = %1").arg(nextVersion)))
            return false;
    }

    return true;
}

QSqlDatabase& Database::handle()
{
    return m_db;
}
