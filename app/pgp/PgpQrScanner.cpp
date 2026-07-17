#include "pgp/PgpQrScanner.h"

#include <ZXing/ImageView.h>
#include <ZXing/ReadBarcode.h>

#include <QDateTime>
#include <QImage>
#include <QVideoFrame>
#include <QVideoSink>

namespace {

// ~3 decode attempts/second -- plenty responsive for a hold-still-to-scan
// flow, far cheaper than running a full ZXing pass on every camera frame
// (a live feed is typically 30-60fps).
constexpr qint64 kDecodeThrottleMs = 300;

// Only the handful of QImage formats QVideoFrame::toImage() actually
// produces on this repo's supported platforms -- anything else falls back
// to a Format_RGB888 conversion (universally supported by QImage, just not
// zero-copy) rather than growing this switch to cover every possible format.
ZXing::ImageFormat imageFormatFor(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGB888:
        return ZXing::ImageFormat::RGB;
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBX8888:
        return ZXing::ImageFormat::RGBA;
    case QImage::Format_Grayscale8:
        return ZXing::ImageFormat::Lum;
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        return ZXing::ImageFormat::BGRA;
#else
        return ZXing::ImageFormat::ARGB;
#endif
    default:
        return ZXing::ImageFormat::None;
    }
}

} // namespace

PgpQrScanner::PgpQrScanner(QObject* parent)
    : QObject(parent)
{
}

void PgpQrScanner::attachSink(QObject* videoSink)
{
    detachSink();

    auto* sink = qobject_cast<QVideoSink*>(videoSink);
    if (!sink)
        return;

    m_connection = connect(sink, &QVideoSink::videoFrameChanged, this, &PgpQrScanner::onVideoFrameChanged);
}

void PgpQrScanner::detachSink()
{
    QObject::disconnect(m_connection);
}

void PgpQrScanner::onVideoFrameChanged(const QVideoFrame& frame)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastDecodeAttemptMs < kDecodeThrottleMs)
        return;
    m_lastDecodeAttemptMs = now;

    if (!frame.isValid())
        return;

    QImage image = frame.toImage();
    if (image.isNull())
        return;

    ZXing::ImageFormat format = imageFormatFor(image.format());
    if (format == ZXing::ImageFormat::None) {
        image = image.convertToFormat(QImage::Format_RGB888);
        format = ZXing::ImageFormat::RGB;
    }

    const ZXing::ImageView view(image.constBits(), image.width(), image.height(), format,
                                 static_cast<int>(image.bytesPerLine()));
    const ZXing::Barcode barcode = ZXing::ReadBarcode(view);
    if (barcode.isValid())
        emit decoded(QString::fromStdString(barcode.text()));
}
