#include "gui_compass.h"
#include "gui_common.h"
#include "mod_gy91.h"
#include "mod_gps.h"
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>

extern Arduino_GFX *gfx;
extern TinyGPSPlus gps;

namespace GUI {
    static float lastHeading = -1.0f;
    // Поточний режим відображення компаса (0..2)
    static int selectedMode = 0;

    void modeNext()
    {
        selectedMode = (selectedMode + 1) % 3;
    }

    void modePrev()
    {
        selectedMode = (selectedMode - 1 + 3) % 3;
    }

    int getSelectedMode()
    {
        return selectedMode;
    }

    void drawVisualCompass()
    {
        drawSubMenuLayout("COMPASS");

        ModuleGY91::NavigationData imu = ModuleGY91::getData();
        float headingFloat = imu.heading;
        if (headingFloat < 0.0f)
            headingFloat += 360.0f;
        if (headingFloat >= 360.0f)
            headingFloat -= 360.0f;
        int headingInt = (int)headingFloat;

        int scaleY = 32;
        int centerX = 80;
        int scaleHeight = 22;

        if (fabs(headingFloat - lastHeading) > 0.2f || lastHeading < 0.0f) {
            lastHeading = headingFloat;
            gfx->fillRect(5, scaleY + 1, 150, scaleHeight - 2, BLACK);
            gfx->drawRect(4, scaleY, 152, scaleHeight, DARKGREEN);

            for (int deg = headingInt - 70; deg <= headingInt + 70; deg += 5) {
                int normalizedDeg = (deg + 360) % 360;
                int screenX = centerX + (deg - headingInt);

                if (screenX >= 6 && screenX <= 154) {
                    if (normalizedDeg % 30 == 0) {
                        gfx->drawFastVLine(screenX, scaleY + 1, 8, GREEN);
                        gfx->setTextSize(1);
                        gfx->setTextColor(WHITE, BLACK);

                        if (normalizedDeg == 0) {
                            gfx->setCursor(screenX - 3, scaleY + 11);
                            gfx->print("N");
                        }
                        else if (normalizedDeg == 90) {
                            gfx->setCursor(screenX - 3, scaleY + 11);
                            gfx->print("E");
                        }
                        else if (normalizedDeg == 180) {
                            gfx->setCursor(screenX - 3, scaleY + 11);
                            gfx->print("S");
                        }
                        else if (normalizedDeg == 270) {
                            gfx->setCursor(screenX - 3, scaleY + 11);
                            gfx->print("W");
                        }
                        else {
                            gfx->setTextColor(RGB565_DARKGREY, BLACK);
                            gfx->setCursor(screenX - 5, scaleY + 11);
                            gfx->printf("%d", normalizedDeg / 10);
                        }
                    }
                    else {
                        gfx->drawFastVLine(screenX, scaleY + 1, 4, RGB565_DARKGREY);
                    }
                }
            }
        }

        gfx->drawTriangle(centerX, scaleY + scaleHeight + 1, centerX - 3, scaleY + scaleHeight + 4, centerX + 3, scaleY + scaleHeight + 4, RED);

        int separatorY = 60;
        gfx->drawFastHLine(0, separatorY, 160, GREEN);

        int dataY = 68;
        int col1X = 6;
        int col2X = 84;

        gfx->fillRect(col1X, dataY, 74, 45, BLACK);
        gfx->fillRect(col2X, dataY, 72, 45, BLACK);

        gfx->setTextColor(YELLOW, BLACK);
        gfx->setCursor(col1X, dataY);
        gfx->printf("DIR:%05.1f\xF7", headingFloat);

        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(col1X, dataY + 14);
        gfx->printf("ALT:%.1fm", imu.altitude);

        float pressureMmHg = imu.pressure * 0.750064f;
        gfx->setCursor(col1X, dataY + 28);
        gfx->printf("BAR:%.1f", pressureMmHg);

        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(col2X, dataY);
        gfx->printf("V_IMU:%.1f", imu.internalSpeed);

        gfx->setTextColor(CYAN, BLACK);
        gfx->setCursor(col2X, dataY + 14);
        if (gps.speed.isValid()) {
            gfx->printf("V_GPS:%.1f", gps.speed.knots() * 0.514444f);
        }
        else {
            gfx->print("V_GPS:0.0");
        }

        drawUniversalFooter();
    }
}
