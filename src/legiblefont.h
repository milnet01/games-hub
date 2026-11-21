#pragma once

#include <QFont>

#include <cmath>

// Growing a font for the legibility switch, shared by every dialog that does
// it. Header-only because it is four lines and has no state.
//
// QFont carries EITHER a point size or a pixel size, and answers -1 for the
// one it is not using. Adding points to a pixel-sized font therefore produced
// a 2.0pt dialog -- microscopic, and worst of all in the very branch meant to
// make the text bigger. Grow whichever unit is actually in force.
//
// Shared rather than copied for exactly that reason: the trap is invisible
// until it bites, and a second copy is a second place for it to come back.
inline void growByPoints(QFont& f, double points)
{
    if (f.pointSizeF() > 0.0) {
        f.setPointSizeF(f.pointSizeF() + points);
        return;
    }
    if (f.pixelSize() > 0) {
        // A point is 1/72 inch against Qt's 96 logical DPI, so roughly 4/3 of
        // a pixel. Exactness does not matter here; not shrinking the text does.
        f.setPixelSize(f.pixelSize() + int(std::lround(points * 4.0 / 3.0)));
    }
}

// Lets Qt keep text up to 256 pixels tall as ready-made letters, where its
// own limit is 64. Past the limit every letter is filled from its outline on
// every paint: on a 4K screen a runner's score is about 100 pixels tall, and
// raising the limit took about 3 ms off each 4K frame (GHUB-0263). Call before
// the QApplication exists, since Qt reads this once; a value already set in
// the environment is left alone.
inline void keepLargeGlyphs()
{
    constexpr const char* name = "QT_MAX_CACHED_GLYPH_SIZE"; // untranslated: an environment variable
    if (!qEnvironmentVariableIsSet(name))
        qputenv(name, "256");
}
