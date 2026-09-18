#include "gui_common.h"
#include "mod_sound.h"
#include "mod_gps.h"
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>

extern Arduino_GFX *gfx;
extern TinyGPSPlus gps;

namespace GUI {

    int getKyivTimeOffset(int year, int month, int day, int hour)
    {
        if (month < 3 || month > 10)
            return 2;
        if (month > 3 && month < 10)
            return 3;
        int lastSunday = 31 - ((5 + year + year / 4 - year / 100 + year / 400) % 7);
        if (month == 3)
        {
            if (day > lastSunday)
                return 3;
            if (day == lastSunday && hour >= 1)
                return 3;
            return 2;
        }
        else
        {
            if (day > lastSunday)
                return 2;
            if (day == lastSunday && hour >= 1)
                return 2;
            return 3;
        }
    }

    float getBatteryVoltage()
    {
        const int potPin = 1; // GPIO 1
        const int samples = 40; 
        
        static float cachedVoltage = 3.6f;
        static unsigned long lastBatteryCheck = 0;

        if (lastBatteryCheck != 0 && (millis() - lastBatteryCheck < 60000)) {
            return cachedVoltage;
        }

        lastBatteryCheck = millis();
        long sum = 0;

        for (int i = 0; i < samples; i++) {
            sum += analogReadMilliVolts(potPin);
            delayMicroseconds(50); 
        }
        
        float averageMv = (float)sum / samples;
        float currentVoltage = (averageMv * 1.2133f) / 1000.0f;

        if (currentVoltage < 2.0f) currentVoltage = 0.0f;
        
        cachedVoltage = currentVoltage;
        
        return cachedVoltage;
    }

    void drawUniversalFooter()
    {
        gfx->drawFastHLine(0, 114, 160, GREEN);
        gfx->fillRect(0, 115, 160, 13, BLACK);

        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(2, 117);
        if (gps.time.isValid() && gps.date.isValid())
        {
            int hour = gps.time.hour();
            int offset = getKyivTimeOffset(gps.date.year(), gps.date.month(), gps.date.day(), hour);
            hour = (hour + offset) % 24;
            gfx->printf("%02d:%02d:%02d", hour, gps.time.minute(), gps.time.second());
        }
        else
        {
            gfx->print("--:--:--");
        }

        float bV = getBatteryVoltage();

        if (bV > 3.7f)
            gfx->setTextColor(YELLOW, BLACK);
        else if (bV > 3.4f)
            gfx->setTextColor(WHITE, BLACK);
        else
            gfx->setTextColor(RED, BLACK);

        gfx->setCursor(118, 117);
        gfx->printf("%.2fV", bV);
    }

    void drawSplash()
    {
        gfx->fillScreen(BLACK);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setTextSize(2);
        gfx->setCursor(12, 40);
        gfx->print("VAMPIRE TOAD");
        gfx->setTextColor(WHITE, BLACK);
        gfx->setTextSize(1);
        gfx->setCursor(44, 65);
        gfx->print("PENTA OS v2.0");

        // КРИТИЧНО: Виштовхуємо canvas на физичний дисплей перед паузою
        gfx->flush();
        
        ModuleSound::play(ModuleSound::SFX_OK);
        delay(1500);
    }

    void drawSubMenuLayout(const char *title)
    {
        gfx->drawRect(0, 0, 160, 128, GREEN);
        gfx->drawFastHLine(0, 15, 160, GREEN);

        gfx->setTextSize(1);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(4, 6);
        gfx->print("VTOAD");

        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(54, 6);
        gfx->print(title); 

        gfx->setCursor(114, 6);
        gfx->print("EXIT >"); 
    }

    void drawHackerIcons(bool isWiFi, bool animate)
    {
        int startX = 134;
        int startY = 18;

        gfx->fillRect(startX, startY, 20, 20, BLACK);

        uint16_t iconColor = (animate && (millis() / 300) % 2 == 0) ? RED : GREEN;

        if (isWiFi)
        {
            gfx->drawTriangle(startX + 10, startY + 12, startX + 7, startY + 19, startX + 13, startY + 19, iconColor);
            gfx->drawFastVLine(startX + 10, startY + 6, 6, iconColor);
            gfx->drawCircle(startX + 10, startY + 6, 4, iconColor);
            gfx->drawCircle(startX + 10, startY + 6, 8, iconColor);
        }
        else
        {
            gfx->drawFastVLine(startX + 10, startY + 1, 18, iconColor);
            gfx->drawLine(startX + 10, startY + 1, startX + 15, startY + 5, iconColor);
            gfx->drawLine(startX + 15, startY + 5, startX + 5, startY + 14, iconColor);
            gfx->drawLine(startX + 5, startY + 5, startX + 15, startY + 14, iconColor);
            gfx->drawLine(startX + 15, startY + 14, startX + 10, startY + 18, iconColor);
        }
    }
}
