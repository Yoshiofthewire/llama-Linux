#include "platform/SecureStoreKeychain.h"

#include <QEventLoop>
#include <qt6keychain/keychain.h>

SecureStoreKeychain::SecureStoreKeychain(const QString& service)
    : m_service(service)
{
}

bool SecureStoreKeychain::set(const QString& key, const QString& value)
{
    QKeychain::WritePasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(value);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    return job.error() == QKeychain::NoError;
}

std::optional<QString> SecureStoreKeychain::get(const QString& key) const
{
    QKeychain::ReadPasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() != QKeychain::NoError)
        return std::nullopt;
    return job.textData();
}

bool SecureStoreKeychain::remove(const QString& key)
{
    QKeychain::DeletePasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    return job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound;
}

bool SecureStoreKeychain::contains(const QString& key) const
{
    return get(key).has_value();
}
