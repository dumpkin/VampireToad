#include "gui_radar.h"
#include "gui_common.h"
#include "mod_gps.h"
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>

extern Arduino_GFX *gfx;
extern TinyGPSPlus gps;

namespace GUI {

    void drawVisualRadarScreen()
    {
        drawSubMenuLayout("  GPS  ");

        ModuleGPS::update();

        gfx->drawFastHLine(0, 100, 160, GREEN);

        gfx->fillRect(2, 102, 156, 11, BLACK);
        gfx->setTextSize(1);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(4, 104);
        gfx->printf("SATS IN USE: %d", gps.satellites.value());

        drawUniversalFooter();
    }
}
