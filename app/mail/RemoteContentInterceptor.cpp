#include "mail/RemoteContentInterceptor.h"

#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInfo>

bool shouldBlockRemoteContentRequest(QWebEngineUrlRequestInfo::ResourceType resourceType, bool imagesLoaded)
{
    if (imagesLoaded)
        return false;
    return resourceType != QWebEngineUrlRequestInfo::ResourceTypeMainFrame;
}

RemoteContentInterceptor::RemoteContentInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

bool RemoteContentInterceptor::imagesLoaded() const
{
    return m_imagesLoaded;
}

void RemoteContentInterceptor::setImagesLoaded(bool loaded)
{
    if (m_imagesLoaded == loaded)
        return;
    m_imagesLoaded = loaded;
    emit imagesLoadedChanged();
}

void RemoteContentInterceptor::installOn(QQuickWebEngineProfile* profile)
{
    if (profile)
        profile->setUrlRequestInterceptor(this);
}

void RemoteContentInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    if (shouldBlockRemoteContentRequest(info.resourceType(), m_imagesLoaded))
        info.block(true);
}
