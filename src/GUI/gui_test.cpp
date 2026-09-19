#include "gui_test.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

extern Arduino_GFX *gfx;

namespace GUI {

    void runGuiTest()
    {
        gfx->fillScreen(BLACK);
        gfx->setTextSize(1);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(4, 8);
        gfx->print("I2C TEST");
        gfx->drawFastHLine(4, 20, 152, GREEN);

        gfx->setCursor(8, 28);
        gfx->setTextColor(WHITE, BLACK);
        gfx->print("Scanning addresses...");
        gfx->flush();

        uint8_t foundAddresses[16];
        size_t foundCount = 0;

        for (uint8_t address = 0x01; address < 0x7F; ++address) {
            Wire.beginTransmission(address);
            const uint8_t error = Wire.endTransmission();
            if (error == 0 && foundCount < sizeof(foundAddresses)) {
                foundAddresses[foundCount++] = address;
            }
        }

        gfx->fillScreen(BLACK);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(4, 8);
        gfx->print("I2C TEST");
        gfx->drawFastHLine(4, 20, 152, GREEN);
        gfx->setCursor(8, 29);
        gfx->setTextColor(WHITE, BLACK);
        gfx->print("Devices found: ");
        gfx->print(foundCount);

        gfx->setCursor(8, 47);
        gfx->setTextColor(GREEN, BLACK);
        if (foundCount == 0) {
            gfx->print("none");
        } else {
            for (size_t index = 0; index < foundCount; ++index) {
                if (index > 0) {
                    gfx->print(" ");
                }
                gfx->printf("0x%02X", foundAddresses[index]);
                if ((index + 1) % 4 == 0 && index + 1 < foundCount) {
                    gfx->setCursor(8, 65 + ((index / 4) * 18));
                }
            }
        }

        gfx->setCursor(8, 106);
        gfx->setTextColor(WHITE, BLACK);
        gfx->print("Scan complete");
        gfx->flush();
        delay(5000);
    }

}