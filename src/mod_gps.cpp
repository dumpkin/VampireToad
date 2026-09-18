#include "mod_gps.h"
#include "mod_sound.h"
#include "gui.h"
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>

extern Arduino_GFX *gfx;
extern TinyGPSPlus gps;

namespace ModuleGPS
{
    int radarRadius = 0;
    const int maxRadius = 40;
    const int centerX = 48;
    const int centerY = 58;
    unsigned long lastRadarTick = 0;
    unsigned long lastTextUpdate = 0;

    void init()
    {
        Serial2.begin(9600, SERIAL_8N1, 18, 17);
        radarRadius = 0;
    }

    void stop()
    {
        radarRadius = 0;
        // Затираємо ліву радарну зону чорним кольором прямо в буфері
        gfx->fillRect(2, 14, 93, 99, BLACK);
    }

    void drawRadarAnimation()
    {
        if (millis() - lastRadarTick >= 50)
        {
            lastRadarTick = millis();

            // Крок анімації під 1-секундний цикл
            radarRadius += 2;

            if (radarRadius > maxRadius)
            {
                radarRadius = 0;
                ModuleSound::playRadarTick(gps.satellites.value());
            }
        }

        // МАЛЮЄМО АНАЛОГОВИЙ ФОСФОРНИЙ ШЛЕЙФ ЗГАСАННЯ ПРЯМО З НУЛЯ
        // Оскільки зона очищується перед кадром, старий хвіст затирається автоматично
        if (radarRadius >= 4)
            gfx->drawCircle(centerX, centerY, radarRadius - 4, RGB565_DARKGREY);
        if (radarRadius >= 3)
            gfx->drawCircle(centerX, centerY, radarRadius - 3, DARKGREEN);
        if (radarRadius >= 2)
            gfx->drawCircle(centerX, centerY, radarRadius - 2, DARKGREEN);
        if (radarRadius >= 1)
            gfx->drawCircle(centerX, centerY, radarRadius - 1, GREEN);
        gfx->drawCircle(centerX, centerY, radarRadius, WHITE);

        // Статичне центральне перехрестя (завжди малюється поверх шлейфів)
        gfx->drawFastHLine(centerX - 5, centerY, 10, GREEN);
        gfx->drawFastVLine(centerX, centerY - 5, 10, GREEN);
    }

    void drawRightPanel()
    {
        gfx->fillRect(97, 14, 62, 99, BLACK);

        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(101, 16);
        gfx->print("SATS:");
        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(134, 16);
        gfx->print(gps.satellites.value());

        int satsCount = gps.satellites.value();
        for (int i = 0; i < 6; i++)
        {
            int posX = 101 + (i * 10);
            if (satsCount > 0 && i < satsCount)
            {
                int barHeight = (15 + (i * 4)) * 0.5;
                if (barHeight > 22)
                    barHeight = 22;
                gfx->fillRect(posX, 62 - barHeight, 5, barHeight, (i == 1 || i == 4) ? RED : GREEN);
            }
            else
            {
                gfx->fillRect(posX, 61, 5, 2, RGB565_DARKGREY);
            }
        }

        gfx->drawFastHLine(97, 68, 63, DARKGREEN);

        gfx->setTextColor(YELLOW, BLACK);
        if (gps.location.isValid())
        {
            gfx->setCursor(101, 74);
            gfx->print("LAT:");
            gfx->setCursor(101, 83);
            gfx->print(gps.location.lat(), 4);
            gfx->setCursor(101, 94);
            gfx->print("LON:");
            gfx->setCursor(101, 103);
            gfx->print(gps.location.lng(), 4);
        }
        else
        {
            gfx->setTextColor(RGB565_DARKGREY, BLACK);
            gfx->setCursor(101, 74);
            gfx->print("NO");
            gfx->setCursor(101, 84);
            gfx->print("POSITION");
        }
    }

    void drawBottomStatusBar()
    {
        gfx->drawFastHLine(0, 114, 160, GREEN);
        gfx->fillRect(0, 115, 160, 13, BLACK);

        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(2, 117);
        if (gps.time.isValid())
        {
            int localHour = (gps.time.hour() + 3) % 24;
            gfx->printf("%02d:%02d:%02d", localHour, gps.time.minute(), gps.time.second());
        }
        else
        {
            gfx->print("00:00:00");
        }

        int totalSats = gps.satellites.value();
        int gpsSats = totalSats * 0.6;
        int bdSats = totalSats - gpsSats;

        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(62, 117);
        gfx->printf("GP:%d", gpsSats);

        gfx->setTextColor(CYAN, BLACK);
        gfx->setCursor(96, 117);
        gfx->printf("BD:%d", bdSats);

        if (gps.location.isValid())
        {
            gfx->setTextColor(GREEN, BLACK);
            gfx->setCursor(126, 117);
            gfx->print("FIX:3D");
        }
        else
        {
            gfx->setTextColor(RED, BLACK);
            gfx->setCursor(126, 117);
            gfx->print("FIX:NO");
        }
    }

    void update()
    {
        while (Serial2.available() > 0)
        {
            gps.encode(Serial2.read());
        }

        // КРИТИЧНО ДЛЯ CANVAS: Чисто очищаємо ліву зону радара перед кожним новим кадром в пам'яті
        gfx->fillRect(2, 14, 93, 99, BLACK);

        // Послідовно рендеримо компоненти у відеобуфер
        drawRadarAnimation();

        if (millis() - lastTextUpdate >= 500)
        {
            lastTextUpdate = millis();
            drawRightPanel();
            drawBottomStatusBar();
        }
    }
}
