#pragma once

#include <QImage>

#include <functional>

class QPainter;

// Painting a picture in bands of rows, one to a core, at once (GHUB-0261).
// Both runners paint their scene this way, since a frame costs by the pixel.
namespace Bands {

// Runs `paint` once a band over `image`, each on a painter that writes only
// that band's rows and is translated so the whole picture's coordinates
// still hold. `paint` must read only what no band writes, and draw no text.
void paint(QImage& image, const std::function<void(QPainter&)>& paint);

} // namespace Bands
