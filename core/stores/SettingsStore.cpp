#include "stores/SettingsStore.h"

namespace {
constexpr auto kThemeIdKey = "appearance/themeId";
constexpr auto kPushServerBaseUrlKey = "notifications/pushServerBaseUrl";
constexpr auto kDeliveryModeKey = "push/deliveryMode";
constexpr auto kPullEndpointKey = "push/pullEndpoint";
constexpr auto kTransportKey = "push/transport";

const QString kDefaultThemeId = QStringLiteral("Patina Ky");

QString keywordVisibleKey(const QString& keyword)
{
    return QStringLiteral("keywords/%1").arg(keyword);
}
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

QString SettingsStore::pushServerBaseUrl() const
{
    return m_settings.value(kPushServerBaseUrlKey, kDefaultPushServerBaseUrl).toString();
}

void SettingsStore::setPushServerBaseUrl(const QString& baseUrl)
{
    m_settings.setValue(kPushServerBaseUrlKey, baseUrl);
}

QString SettingsStore::deliveryMode() const
{
    return m_settings.value(kDeliveryModeKey, QString()).toString();
}

void SettingsStore::setDeliveryMode(const QString& mode)
{
    m_settings.setValue(kDeliveryModeKey, mode);
}

QString SettingsStore::pullEndpoint() const
{
    return m_settings.value(kPullEndpointKey, QString()).toString();
}

void SettingsStore::setPullEndpoint(const QString& endpoint)
{
    m_settings.setValue(kPullEndpointKey, endpoint);
}

QString SettingsStore::transport() const
{
    return m_settings.value(kTransportKey, QString()).toString();
}

void SettingsStore::setTransport(const QString& transport)
{
    m_settings.setValue(kTransportKey, transport);
}

bool SettingsStore::keywordVisible(const QString& keyword) const
{
    return m_settings.value(keywordVisibleKey(keyword), true).toBool();
}

void SettingsStore::setKeywordVisible(const QString& keyword, bool visible)
{
    m_settings.setValue(keywordVisibleKey(keyword), visible);
}
