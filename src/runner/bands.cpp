#include "bands.h"

#include <QPainter>
#include <QThread>
#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>
#include <numeric>
#include <vector>

namespace Bands {
namespace {
// The most bands at once: past this, a band is too short for another core
// to earn its keep.
constexpr int kMostBands = 8;
} // namespace

void paint(QImage& image, const std::function<void(QPainter&)>& paint)
{
    uchar* bits = image.bits();
    const qsizetype stride = image.bytesPerLine();
    const int rows = image.height();
    std::vector<int> bands(size_t(std::clamp(QThread::idealThreadCount(), 1, kMostBands)));
    std::iota(bands.begin(), bands.end(), 0);
    QtConcurrent::blockingMap(bands, [&](int band) {
        const int count = int(bands.size());
        const int top = rows * band / count;
        const int bottom = rows * (band + 1) / count;
        if (bottom <= top)
            return;
        QImage part(bits + top * stride, image.width(), bottom - top, stride, image.format());
        QPainter q(&part);
        q.translate(0, -top);
        paint(q);
    });
}

} // namespace Bands
