#include "platform/SecureStoreKeychain.h"

#include <QEventLoop>
#include <qt6keychain/keychain.h>

SecureStoreKeychain::SecureStoreKeychain(const QString& service)
    : m_service(service)
{
}

// Shared connect-start-exec pattern every job below needs: runs `job`
// synchronously on a local QEventLoop, quitting it on QKeychain::Job's
// finished signal, and returns once the job has completed. Callers inspect
// job.error()/job.textData() themselves afterward -- this only owns the
// blocking-wait mechanics, not any per-job error interpretation.
bool SecureStoreKeychain::runBlocking(QKeychain::Job& job)
{
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    return job.error() == QKeychain::NoError;
}

bool SecureStoreKeychain::set(const QString& key, const QString& value)
{
    QKeychain::WritePasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(value);

    return runBlocking(job);
}

std::optional<QString> SecureStoreKeychain::get(const QString& key) const
{
    QKeychain::ReadPasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);

    if (!runBlocking(job))
        return std::nullopt;
    return job.textData();
}

bool SecureStoreKeychain::remove(const QString& key)
{
    QKeychain::DeletePasswordJob job(m_service);
    job.setAutoDelete(false);
    job.setKey(key);

    if (runBlocking(job))
        return true;
    return job.error() == QKeychain::EntryNotFound;
}

bool SecureStoreKeychain::contains(const QString& key) const
{
    return get(key).has_value();
}
