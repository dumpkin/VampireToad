#include "mod_test.h"
#include "mod_buttons.h"
#include <Wire.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

namespace ModuleTest {
    bool test_active = false;

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

    void captureSplashTestRequest(bool keypadReady, unsigned long durationMs)
    {
        test_active = false;
        const unsigned long startedAt = millis();
        unsigned long hashPressedAt = 0;
        while (millis() - startedAt < durationMs) {
            if (keypadReady && ModuleButtons::isKeyDown('#')) {
                if (hashPressedAt == 0) hashPressedAt = millis();
                if (millis() - hashPressedAt >= 25UL) test_active = true;
            } else {
                hashPressedAt = 0;
            }
            delay(5);
        }
    }

    static void drawKeypadTestScreen(uint16_t pressedKeys, uint16_t testedKeys)
    {
        gfx->fillScreen(BLACK);
        gfx->setTextSize(1);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(37, 16);
        gfx->print("KEYPAD TEST");
        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(20, 27);
        gfx->print("Press every key");

        for (uint8_t index = 0; index < 16; ++index) {
            const uint8_t column = index % 4;
            const uint8_t row = index / 4;
            const int x = 10 + column * 36;
            const int y = 39 + row * 18;
            const bool tested = (testedKeys & (1U << index)) != 0;
            const bool pressed = (pressedKeys & (1U << index)) != 0;
            const uint16_t background = pressed ? YELLOW : (tested ? GREEN : BLACK);
            const uint16_t foreground = (pressed || tested) ? BLACK : WHITE;

            gfx->fillRect(x, y, 32, 16, background);
            gfx->drawRect(x, y, 32, 16, tested ? GREEN : RGB565_DARKGREY);
            gfx->setTextColor(foreground, background);
            gfx->setCursor(x + 13, y + 4);
            gfx->print(ModuleButtons::keySymbol(index));
        }

        char currentSymbol = '-';
        for (uint8_t index = 0; index < 16; ++index) {
            if (pressedKeys & (1U << index)) {
                currentSymbol = ModuleButtons::keySymbol(index);
                break;
            }
        }

        gfx->setTextColor(WHITE, BLACK);
        gfx->setCursor(12, 113);
        gfx->printf("READ: %c", currentSymbol);
        uint8_t testedCount = 0;
        for (uint8_t index = 0; index < 16; ++index) {
            if (testedKeys & (1U << index)) ++testedCount;
        }
        gfx->setCursor(91, 113);
        gfx->printf("%u/16", static_cast<unsigned>(testedCount));
    }

    void runKeypadTest()
    {
        uint16_t testedKeys = 0;
        uint16_t candidateKeys = 0;
        uint16_t stableKeys = 0;
        uint16_t lastDrawnPressed = 0xFFFF;
        uint16_t lastDrawnTested = 0xFFFF;
        unsigned long candidateChangedAt = millis();

        drawKeypadTestScreen(stableKeys, testedKeys);
        gfx->flush();

        while (testedKeys != 0xFFFF) {
            const unsigned long now = millis();
            const uint16_t sampledKeys = ModuleButtons::readMatrix();
            if (sampledKeys != candidateKeys) {
                candidateKeys = sampledKeys;
                candidateChangedAt = now;
            } else if (candidateKeys != stableKeys &&
                       now - candidateChangedAt >= 25UL) {
                stableKeys = candidateKeys;
            }

            const uint16_t newlyPressed = stableKeys & ~testedKeys;
            if (newlyPressed != 0) {
                testedKeys |= newlyPressed;
                for (uint8_t index = 0; index < 16; ++index) {
                    if (newlyPressed & (1U << index)) {
                        Serial.printf("[Keypad test] detected '%c' (column %u, row %u)\n",
                                      ModuleButtons::keySymbol(index),
                                      static_cast<unsigned>(index % 4),
                                      static_cast<unsigned>(index / 4));
                    }
                }
            }

            if (stableKeys != lastDrawnPressed || testedKeys != lastDrawnTested) {
                drawKeypadTestScreen(stableKeys, testedKeys);
                gfx->flush();
                lastDrawnPressed = stableKeys;
                lastDrawnTested = testedKeys;
            }

            delay(5);
        }

        gfx->fillRect(0, 110, 160, 18, BLACK);
        gfx->setTextColor(GREEN, BLACK);
        gfx->setCursor(38, 113);
        gfx->print("TEST COMPLETE");
        gfx->flush();
        delay(700);
        ModuleButtons::resetAndIgnore(500UL);
    }

    void runI2CScan()
    {
        uint8_t foundAddresses[16];
        size_t foundCount = 0;
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
                if (index > 0) gfx->print(" ");
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
