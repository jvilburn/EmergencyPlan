#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

#include "QoiCodec.h"

#include <cstring>
#include <cstdlib>

namespace QoiCodec
{

QByteArray encode(const QImage& image)
{
    if (image.isNull())
    {
        return {};
    }

    // Convert to RGB888 format (24-bit, no alpha)
    QImage rgb = image.format() == QImage::Format_RGB888
        ? image
        : image.convertToFormat(QImage::Format_RGB888);

    qoi_desc desc;
    desc.width = rgb.width();
    desc.height = rgb.height();
    desc.channels = 3;
    desc.colorspace = QOI_SRGB;

    int outLen = 0;
    void* encoded = qoi_encode(rgb.constBits(), &desc, &outLen);
    if (!encoded)
    {
        return {};
    }

    QByteArray result(reinterpret_cast<const char*>(encoded), outLen);
    free(encoded);
    return result;
}

QImage decode(const QByteArray& data)
{
    if (data.isEmpty())
    {
        return {};
    }

    qoi_desc desc;
    // Decode as 3 channels (RGB) - smallest data, fastest decode
    void* pixels = qoi_decode(data.constData(), data.size(), &desc, 3);
    if (!pixels)
    {
        return {};
    }

    // Wrap buffer and use Qt's optimized copy (faster than manual memcpy)
    QImage wrapper(
        reinterpret_cast<const uchar*>(pixels),
        static_cast<int>(desc.width),
        static_cast<int>(desc.height),
        static_cast<int>(desc.width * 3),
        QImage::Format_RGB888);
    QImage result = wrapper.copy();
    free(pixels);

    return result;
}

}  // namespace QoiCodec
