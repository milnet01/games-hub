#include "gameview.h"

// GameView is pure interface apart from its signal, but a Q_OBJECT class still
// needs a translation unit of its own: AUTOMOC only generates the metaobject
// for a header that has a matching source file in the build.
