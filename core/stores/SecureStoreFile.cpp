#include "stores/SecureStoreFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

SecureStoreFile::SecureStoreFile(const QString& directoryPath)
    : m_directoryPath(directoryPath)
{
}

QString SecureStoreFile::filePathFor(const QString& key) const
{
    return QDir(m_directoryPath).filePath(key);
}

bool SecureStoreFile::set(const QString& key, const QString& value)
{
    QFile file(filePathFor(key));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const bool wrote = file.write(value.toUtf8()) >= 0;
    file.close();
    if (!wrote)
        return false;

    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

std::optional<QString> SecureStoreFile::get(const QString& key) const
{
    QFile file(filePathFor(key));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    return QString::fromUtf8(file.readAll());
}

bool SecureStoreFile::remove(const QString& key)
{
    QFile file(filePathFor(key));
    if (!file.exists())
        return true;
    return file.remove();
}

bool SecureStoreFile::contains(const QString& key) const
{
    return QFileInfo::exists(filePathFor(key));
}
