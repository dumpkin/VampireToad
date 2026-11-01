#include "gui_navigation.h"
#include "gui_common.h"
#include "mod_gps.h"
#include "mod_gy91.h"
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>
#include <math.h>

extern Arduino_GFX *gfx;
extern TinyGPSPlus gps;

namespace GUI {
    namespace {
        enum class Metric : uint8_t {
            SKYMAP,
            HEADING,
            ATMOSPHERE,
            BAROMETRIC_ALTITUDE,
            IMU_SPEED,
            GPS_DATA,
            SATELLITES,
            COUNT
        };

        uint8_t selectedMetric = 0;
        constexpr int SKY_CENTER_X = 49;
        constexpr int SKY_CENTER_Y = 68;
        constexpr int SKY_RADIUS = 40;
        constexpr uint8_t MAX_SATELLITES = 32;

        uint16_t signalColor(uint8_t strength)
        {
            if (strength >= 35) return GREEN;
            if (strength >= 20) return YELLOW;
            return RED;
        }

        void bearingPosition(float bearing, int radius, int &x, int &y)
        {
            const float radians = bearing * 0.0174532925f;
            x = SKY_CENTER_X + static_cast<int>(sinf(radians) * radius);
            y = SKY_CENTER_Y - static_cast<int>(cosf(radians) * radius);
        }

        void drawBearingLabel(const char *label, float bearing, float heading)
        {
            int x;
            int y;
            bearingPosition(bearing - heading, SKY_RADIUS - 7, x, y);
            gfx->setCursor(x - 2, y - 3);
            gfx->print(label);
        }

        void drawSkySatellite(const ModuleGPS::Satellite &satellite, float heading)
        {
            const float relativeAzimuth = satellite.azimuth - heading;
            const float radius = SKY_RADIUS * (90.0f - satellite.elevation) / 90.0f;
            int x;
            int y;
            bearingPosition(relativeAzimuth, static_cast<int>(radius), x, y);
            const uint16_t color = signalColor(satellite.signalStrength);

            switch (satellite.constellation) {
                case ModuleGPS::Constellation::GLONASS:
                    gfx->fillRect(x - 2, y - 2, 5, 5, color);
                    break;
                case ModuleGPS::Constellation::BEIDOU:
                    gfx->fillTriangle(x, y - 3, x - 3, y + 2, x + 3, y + 2, color);
                    break;
                case ModuleGPS::Constellation::GPS:
                    gfx->fillCircle(x, y, 2, color);
                    break;
            }
        }

        void drawSkyMap(const ModuleGY91::NavigationData &imu)
        {
            const float heading = ModuleGY91::isSensorReady() && isfinite(imu.heading)
                ? imu.heading
                : 0.0f;

            gfx->drawCircle(SKY_CENTER_X, SKY_CENTER_Y, SKY_RADIUS, DARKGREEN);
            gfx->drawCircle(SKY_CENTER_X, SKY_CENTER_Y, SKY_RADIUS / 2, RGB565_DARKGREY);
            int northX;
            int northY;
            int eastX;
            int eastY;
            bearingPosition(-heading, SKY_RADIUS, northX, northY);
            bearingPosition(90.0f - heading, SKY_RADIUS, eastX, eastY);
            gfx->drawLine(northX, northY, 2 * SKY_CENTER_X - northX,
                          2 * SKY_CENTER_Y - northY, RGB565_DARKGREY);
            gfx->drawLine(eastX, eastY, 2 * SKY_CENTER_X - eastX,
                          2 * SKY_CENTER_Y - eastY, RGB565_DARKGREY);

            gfx->setTextSize(0);
            gfx->setTextColor(WHITE, BLACK);
            drawBearingLabel("N", 0.0f, heading);
            drawBearingLabel("E", 90.0f, heading);
            drawBearingLabel("S", 180.0f, heading);
            drawBearingLabel("W", 270.0f, heading);

            ModuleGPS::Satellite satellites[MAX_SATELLITES];
            const uint8_t satelliteCount = ModuleGPS::copySatellites(
                satellites, MAX_SATELLITES);
            for (uint8_t i = 0; i < satelliteCount; ++i) {
                drawSkySatellite(satellites[i], heading);
            }

            gfx->drawFastVLine(98, 21, 88, DARKGREEN);
            gfx->setTextSize(0);
            gfx->setTextColor(GREEN, BLACK);
            gfx->setCursor(103, 23);
            gfx->print("SV:");
            gfx->setTextColor(WHITE, BLACK);
            gfx->setCursor(125, 23);
            gfx->print(ModuleGPS::satellitesInView());

            const ModuleGPS::LastFix lastFix = ModuleGPS::getLastFix();
            const bool livePosition = gps.location.isValid();
            gfx->setTextColor(livePosition ? GREEN : YELLOW, BLACK);
            gfx->setCursor(103, 31);
            gfx->print(livePosition ? "LIVE" : lastFix.valid ? "MEM" : "NO FIX");

            gfx->setTextColor(CYAN, BLACK);
            gfx->setCursor(103, 39);
            gfx->print("HDG");
            gfx->setTextColor(WHITE, BLACK);
            gfx->setCursor(103, 49);
            if (ModuleGY91::isSensorReady() && isfinite(imu.heading))
                gfx->printf("%03.0f", imu.heading);
            else
                gfx->print("---");

            gfx->setTextColor(YELLOW, BLACK);
            gfx->setCursor(103, 65);
            gfx->print("LAT:");
            gfx->setCursor(103, 75);
            if (livePosition)
                gfx->printf("%.4f", gps.location.lat());
            else if (lastFix.valid)
                gfx->printf("%.4f", lastFix.latitude);
            else
                gfx->print("--");

            gfx->setCursor(103, 89);
            gfx->print("LON:");
            gfx->setCursor(103, 99);
            if (livePosition)
                gfx->printf("%.4f", gps.location.lng());
            else if (lastFix.valid)
                gfx->printf("%.4f", lastFix.longitude);
            else
                gfx->print("--");
        }

        void drawMetricPage(Metric metric, const ModuleGY91::NavigationData &imu)
        {
            gfx->setTextSize(0);
            gfx->setTextColor(WHITE, BLACK);

            switch (metric) {
                case Metric::HEADING:
                    gfx->setCursor(12, 52);
                    gfx->print("HEADING ");
                    gfx->setTextSize(2);
                    gfx->setCursor(12, 64);
                    if (ModuleGY91::isSensorReady() && isfinite(imu.heading))
                        gfx->printf("%.1f deg", imu.heading);
                    else
                        gfx->print("---");
                    break;
                case Metric::ATMOSPHERE:
                    gfx->setCursor(12, 48);
                    gfx->print("TEMPERATURE");
                    gfx->setTextSize(2);
                    gfx->setCursor(12, 58);
                    if (ModuleGY91::isSensorReady() && isfinite(imu.temperature))
                        gfx->printf("%.1f C", imu.temperature);
                    else
                        gfx->print("---");

                    gfx->setTextSize(0);
                    gfx->setCursor(12, 80);
                    gfx->print("PRESSURE");
                    gfx->setTextSize(2);
                    gfx->setCursor(12, 90);
                    if (ModuleGY91::isSensorReady() && isfinite(imu.pressure))
                        gfx->printf("%.1f mmHg", imu.pressure * 0.7500617f);
                    else
                        gfx->print("---");
                    break;
                case Metric::BAROMETRIC_ALTITUDE:
                    gfx->setCursor(12, 52);
                    gfx->print("ALTITUDE ");
                    gfx->setTextSize(2);
                    gfx->setCursor(12, 64);
                    if (ModuleGY91::isSensorReady() && isfinite(imu.altitude))
                        gfx->printf("%.1f m", imu.altitude);
                    else
                        gfx->print("---");
                    break;
                case Metric::IMU_SPEED:
                    gfx->setCursor(12, 52);
                    gfx->print("IMU SPEED ");
                    gfx->setTextSize(2);
                    gfx->setCursor(12, 64);
                    if (ModuleGY91::isSensorReady() && isfinite(imu.internalSpeed))
                        gfx->printf("%.2f m/s", imu.internalSpeed);
                    else
                        gfx->print("---");
                    break;
                case Metric::GPS_DATA:
                {
                    const ModuleGPS::LastFix lastFix = ModuleGPS::getLastFix();
                    const bool livePosition = gps.location.isValid();

                    gfx->setCursor(12, 58);
                    gfx->print("LAT ");
                    if (livePosition) {
                        gfx->printf("%.5f", gps.location.lat());
                    } else if (lastFix.valid) {
                        gfx->printf("%.5f", lastFix.latitude);
                    } else {
                        gfx->print("---");
                    }
                    gfx->setCursor(12, 78);
                    gfx->print("LON ");
                    if (livePosition)
                        gfx->printf("%.5f", gps.location.lng());
                    else if (lastFix.valid)
                        gfx->printf("%.5f", lastFix.longitude);
                    else
                        gfx->print("---");
                    break;
                }
                case Metric::SATELLITES:
                    gfx->setCursor(12, 54);
                    gfx->print("VISIBLE ");
                    gfx->print(ModuleGPS::satellitesInView());
                    break;
                case Metric::SKYMAP:
                case Metric::COUNT:
                    break;
            }
        }
    }

    void drawNavigationScreen()
    {
        drawSubMenuLayout("GPS-COMP");
        gfx->fillRect(1, 16, 158, 98, BLACK);

        const ModuleGY91::NavigationData imu = ModuleGY91::getData();
        const Metric metric = static_cast<Metric>(selectedMetric);
        static const char *metricLabels[] = {
            "SKYMAP",
            "COMPASS HEADING",
            "PRESSURE/TEMPERATURE",
            "BAROMETRIC ALTITUDE",
            "IMU SPEED",
            "GPS POSITION",
            "SATELLITES IN VIEW"
        };

        if (metric == Metric::SKYMAP) {
            drawSkyMap(imu);
        } else {
            gfx->setTextSize(0);
            gfx->setTextColor(CYAN, BLACK);
            gfx->setCursor(12, 31);
            gfx->print(metricLabels[selectedMetric]);
            gfx->drawFastHLine(12, 43, 136, DARKGREEN);
            drawMetricPage(metric, imu);
        }

        drawUniversalFooter();
    }

    void navigationNextMetric()
    {
        selectedMetric = (selectedMetric + 1) % static_cast<uint8_t>(Metric::COUNT);
    }

    void navigationPreviousMetric()
    {
        selectedMetric = (selectedMetric + static_cast<uint8_t>(Metric::COUNT) - 1) %
                         static_cast<uint8_t>(Metric::COUNT);
    }
}
