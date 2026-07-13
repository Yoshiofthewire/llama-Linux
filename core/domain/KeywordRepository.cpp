#include "domain/KeywordRepository.h"

#include "stores/SettingsStore.h"

#include <QMap>
#include <algorithm>

KeywordRepository::KeywordRepository(SettingsStore& settingsStore)
    : m_settingsStore(settingsStore)
{
}

QVector<KeywordTab> KeywordRepository::computeTabs(const QVector<Email>& emails)
{
    // QMap<QString, int> rather than QHash so ties on count don't affect the
    // final sort's stability in any surprising way; the sort below is the
    // actual ordering authority regardless.
    QMap<QString, int> counts;
    for (const Email& email : emails) {
        for (const QString& keyword : email.keywords)
            counts[keyword] += 1;
    }

    QVector<KeywordTab> tabs;
    tabs.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        tabs.append({ it.key(), it.value() });

    std::sort(tabs.begin(), tabs.end(), [](const KeywordTab& a, const KeywordTab& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return tabs;
}

QVector<KeywordTab> KeywordRepository::visibleTabs(const QVector<Email>& emails) const
{
    QVector<KeywordTab> visible;
    for (const KeywordTab& tab : computeTabs(emails)) {
        if (m_settingsStore.keywordVisible(tab.name))
            visible.append(tab);
    }
    return visible;
}

QVector<KeywordSettings> KeywordRepository::allSettings(const QVector<Email>& emails) const
{
    QVector<KeywordSettings> settings;
    for (const KeywordTab& tab : computeTabs(emails)) {
        KeywordSettings entry;
        entry.keyword = tab.name;
        entry.visible = m_settingsStore.keywordVisible(tab.name);
        settings.append(entry);
    }
    return settings;
}

void KeywordRepository::setVisible(const QString& keyword, bool visible)
{
    m_settingsStore.setKeywordVisible(keyword, visible);
}
