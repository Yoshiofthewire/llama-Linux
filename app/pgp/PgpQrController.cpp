#include "pgp/PgpQrController.h"

#include "domain/PgpQrRepository.h"
#include "net/NetworkError.h"
#include "net/PgpQrClient.h"

#include <KLocalizedString>

// Plain ZXing:: C++ API (not ZXingQt.h's Q_GADGET/Q_OBJECT wrapper types --
// those need moc processing, which AUTOMOC won't reach into for a vendored
// system header; see app/CMakeLists.txt's PgpQrController comment).
#include <ZXing/CreateBarcode.h>
#include <ZXing/WriteBarcode.h>

#include <QBuffer>
#include <QHostAddress>
#include <QImage>
#include <QUrl>

namespace {

// Blocks the two classes of scanQrPayload() target that would turn a QR
// scan into something worse than "fetch a key from wherever it points":
// non-http(s) schemes (file:// would read local files back as if they were
// key material) and link-local/cloud-metadata addresses (169.254.0.0/16 --
// AWS/Azure/DigitalOcean's metadata IP, plus GCP's metadata.google.internal
// hostname). Arbitrary *http(s)* hosts, including LAN/private IPs, are
// intentionally still allowed -- scanQrPayload()'s own comment below notes
// the scanned server is expected to often be a different, independently
// self-hosted Relay instance than this device's own paired one, and
// self-hosted instances commonly live on a home LAN or even localhost (see
// PgpQrControllerTest's FakeRelayServer fixtures, which target 127.0.0.1).
bool isSafeQrTarget(const QUrl& url)
{
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)
        return false;

    const QString host = url.host();
    if (host.isEmpty())
        return false;
    if (host.compare(QStringLiteral("metadata.google.internal"), Qt::CaseInsensitive) == 0)
        return false;

    QHostAddress addr;
    if (addr.setAddress(host) && addr.isLinkLocal())
        return false;

    return true;
}

} // namespace

PgpQrController::PgpQrController(PgpQrRepository& repository, PgpQrClient& client, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_client(client)
{
}

bool PgpQrController::isBusy() const
{
    return m_isBusy;
}

QString PgpQrController::lastError() const
{
    return m_lastError;
}

QString PgpQrController::myQrUrl() const
{
    return m_myQrUrl;
}

QString PgpQrController::myQrExpiresAt() const
{
    return m_myQrExpiresAt;
}

QString PgpQrController::scannedName() const
{
    return m_scannedName;
}

QString PgpQrController::scannedFingerprint() const
{
    return m_scannedFingerprint;
}

QString PgpQrController::scannedPublicKey() const
{
    return m_scannedPublicKey;
}

void PgpQrController::setBusy(bool busy)
{
    if (m_isBusy == busy)
        return;
    m_isBusy = busy;
    emit isBusyChanged();
}

void PgpQrController::setLastError(const QString& error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void PgpQrController::refreshMyQrCode()
{
    setBusy(true);
    const PgpQrTokenOutcome outcome = m_repository.fetchMyToken();
    setBusy(false);

    switch (outcome.status) {
    case PgpQrTokenStatus::Success:
        setLastError(QString());
        m_myQrUrl = outcome.url;
        m_myQrExpiresAt = outcome.expiresAt;
        emit myQrDataChanged();
        break;
    case PgpQrTokenStatus::NotPaired:
        setLastError(i18n("Not paired"));
        break;
    case PgpQrTokenStatus::NoPgpIdentity:
        setLastError(i18n("You haven't set up PGP encryption yet"));
        break;
    case PgpQrTokenStatus::Unauthorized:
        setLastError(i18n("Session expired -- please re-pair this device"));
        break;
    case PgpQrTokenStatus::ServiceUnavailable:
        setLastError(i18n("PGP QR exchange is unavailable right now"));
        break;
    case PgpQrTokenStatus::Retry:
        setLastError(outcome.detail.isEmpty() ? i18n("Failed to load QR code, try again") : outcome.detail);
        break;
    }
}

QString PgpQrController::myQrImageDataUrl() const
{
    if (m_myQrUrl.isEmpty())
        return QString();

    const ZXing::CreatorOptions creatorOptions(ZXing::BarcodeFormat::QRCode);
    const ZXing::Barcode barcode = ZXing::CreateBarcodeFromText(m_myQrUrl.toStdString(), creatorOptions);
    if (!barcode.isValid())
        return QString();

    // WriteBarcodeToImage renders single-byte-per-pixel (Lum/grayscale)
    // data with no padding between rows -- width itself is the correct
    // bytesPerLine, same as zxing-cpp's own ZXingQt::Barcode::toImage()
    // reference implementation. .copy() takes ownership of the pixel data
    // before ZXing::Image (and the buffer QImage was only a view over) goes
    // out of scope.
    const ZXing::Image zxImage = ZXing::WriteBarcodeToImage(barcode);
    const QImage image =
        QImage(zxImage.data(), zxImage.width(), zxImage.height(), zxImage.width(), QImage::Format_Grayscale8).copy();

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(pngBytes.toBase64());
}

void PgpQrController::scanQrPayload(const QString& decodedText)
{
    // No RelayAuth -- the token in the URL is the sole credential, and the
    // scan target may be a different server than this device's own paired
    // one, so this deliberately doesn't route through m_repository at all.
    const QUrl qrUrl(decodedText);
    if (!qrUrl.isValid() || !isSafeQrTarget(qrUrl) || !qrUrl.path().contains(QStringLiteral("/api/pgp/qr/key"))) {
        setLastError(i18n("That QR code isn't a PGP key-exchange code"));
        return;
    }

    setBusy(true);
    const PgpQrKeyResult result = m_client.fetchKey(qrUrl);
    setBusy(false);

    if (!result.error.has_value()) {
        setLastError(QString());
        m_scannedName = result.name;
        m_scannedFingerprint = result.fingerprint;
        m_scannedPublicKey = result.publicKey;
        emit scanResultChanged();
        return;
    }

    if (result.statusCode == 404) {
        setLastError(i18n("This person hasn't set up PGP encryption yet"));
    } else if (*result.error == NetworkError::Unauthorized) {
        // 403 here means "invalid or expired token" (handlePGPQRKey), not
        // this device's own auth -- distinct wording from the token-fetch
        // side's 401 handling above.
        setLastError(i18n("That code has expired -- ask them to refresh and scan again"));
    } else if (*result.error == NetworkError::ServiceUnavailable) {
        setLastError(i18n("PGP QR exchange is unavailable right now"));
    } else {
        setLastError(result.detail.isEmpty() ? i18n("Failed to fetch key, try again") : result.detail);
    }
}

void PgpQrController::clearScanResult()
{
    setLastError(QString());
    m_scannedName.clear();
    m_scannedFingerprint.clear();
    m_scannedPublicKey.clear();
    emit scanResultChanged();
}
