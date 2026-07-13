#pragma once

#include "stores/SecureStore.h"

#include <QString>

// Writes each key as a separate file under a caller-supplied directory.
// Every file is chmod'd to owner-read-write only (0600) on write, using the
// portable QFileDevice flags so behavior is identical on Qt5/Qt6. The
// directory itself is not created here on construction — callers (or a
// future phase's platform glue) are expected to pass an existing directory
// resolved via QStandardPaths.
class SecureStoreFile : public SecureStore
{
public:
    explicit SecureStoreFile(const QString& directoryPath);

    bool set(const QString& key, const QString& value) override;
    std::optional<QString> get(const QString& key) const override;
    bool remove(const QString& key) override;
    bool contains(const QString& key) const override;

private:
    QString filePathFor(const QString& key) const;

    QString m_directoryPath;
};
