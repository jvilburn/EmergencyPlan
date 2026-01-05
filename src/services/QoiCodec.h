#pragma once

#include <QImage>
#include <QByteArray>

/// Qt wrapper for QOI (Quite OK Image) format.
/// QOI is ~10x faster to decode than PNG with similar compression.
namespace QoiCodec
{
    /// Encode a QImage to QOI format.
    /// Returns empty QByteArray on failure.
    QByteArray encode(const QImage& image);

    /// Decode QOI data to a QImage.
    /// Returns null QImage on failure.
    QImage decode(const QByteArray& data);
}
