#pragma once

#include <QMetaObject>
#include <QObject>
#include <QString>

class QVideoFrame;

// Taps the live camera frame stream a QML VideoOutput is already rendering
// (via its videoSink property) and decodes QR codes out of it, throttled to
// avoid running a full decode pass on every single frame. Registered as a
// creatable QML type ("PgpQrScanner") in main.cpp -- this repo's first
// creatable (non-singleton) QML-registered C++ type.
//
// Uses the plain ZXing:: C++ API (not ZXingQt.h's Q_GADGET/Q_OBJECT wrapper
// types, which need moc processing AUTOMOC won't reach into for a vendored
// system header -- see app/CMakeLists.txt's PgpQrController comment for the
// same issue on the encode side).
class PgpQrScanner : public QObject
{
    Q_OBJECT

public:
    explicit PgpQrScanner(QObject* parent = nullptr);

    // videoSink must be a QVideoSink* (VideoOutput.videoSink from QML) --
    // silently ignored otherwise.
    Q_INVOKABLE void attachSink(QObject* videoSink);
    Q_INVOKABLE void detachSink();

signals:
    void decoded(const QString& text);

private slots:
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    QMetaObject::Connection m_connection;
    qint64 m_lastDecodeAttemptMs = 0;
};
