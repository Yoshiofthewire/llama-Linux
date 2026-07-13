#include "stores/SettingsStore.h"

namespace {
constexpr auto kThemeIdKey = "appearance/themeId";
constexpr auto kManualMobileOverrideKey = "appearance/manualMobileOverride";
constexpr auto kPushServerBaseUrlKey = "notifications/pushServerBaseUrl";

const QString kDefaultThemeId = QStringLiteral("Dark Matter");
const QString kDefaultPushServerBaseUrl = QStringLiteral("https://ntfy.sh");
} // namespace

SettingsStore::SettingsStore(const QString& filePath)
    : m_settings(filePath, QSettings::IniFormat)
{
}

QString SettingsStore::themeId() const
{
    return m_settings.value(kThemeIdKey, kDefaultThemeId).toString();
}

void SettingsStore::setThemeId(const QString& themeId)
{
    m_settings.setValue(kThemeIdKey, themeId);
}

std::optional<bool> SettingsStore::manualMobileOverride() const
{
    if (!m_settings.contains(kManualMobileOverrideKey))
        return std::nullopt;
    return m_settings.value(kManualMobileOverrideKey).toBool();
}

void SettingsStore::setManualMobileOverride(std::optional<bool> override)
{
    if (override.has_value())
        m_settings.setValue(kManualMobileOverrideKey, override.value());
    else
        m_settings.remove(kManualMobileOverrideKey);
}

QString SettingsStore::pushServerBaseUrl() const
{
    return m_settings.value(kPushServerBaseUrlKey, kDefaultPushServerBaseUrl).toString();
}

void SettingsStore::setPushServerBaseUrl(const QString& baseUrl)
{
    m_settings.setValue(kPushServerBaseUrlKey, baseUrl);
}
