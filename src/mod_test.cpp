#include "mod_test.h"
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

namespace ModuleTest {
    void runDisplayColorPalette()
    {
        // Future debug block for display hardware validation.
        // Kept disabled in production by default.
        //
        // gfx->fillScreen(BLACK);
        // gfx->fillRect(0, 0, 128, 20, RED);
        // gfx->fillRect(0, 20, 128, 20, YELLOW);
        // gfx->fillRect(0, 40, 128, 20, GREEN);
        // gfx->fillRect(0, 60, 128, 20, BLUE);
        // gfx->flush();
    }
}
