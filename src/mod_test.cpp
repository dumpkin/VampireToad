#include "mod_test.h"
#include "mod_buttons.h"
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>

extern Arduino_GFX *gfx;

namespace ModuleTest {
    namespace {
        constexpr size_t PSRAM_TEST_BYTES = 16U * 1024U;

        enum class PsramTestStatus {
            NotFound,
            AllocationFailed,
            Passed,
            Failed
        };

        struct PsramTestResult {
            PsramTestStatus status;
            size_t totalBytes;
            size_t freeBytes;
            size_t failedWord;
        };

        PsramTestResult testPsram()
        {
            PsramTestResult result = {PsramTestStatus::NotFound, 0, 0, 0};
            if (!psramFound()) return result;

            result.totalBytes = ESP.getPsramSize();
            result.freeBytes = ESP.getFreePsram();
            uint32_t *buffer = static_cast<uint32_t *>(
                heap_caps_malloc(PSRAM_TEST_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (buffer == nullptr) {
                result.status = PsramTestStatus::AllocationFailed;
                return result;
            }

            result.status = PsramTestStatus::Passed;
            const size_t wordCount = PSRAM_TEST_BYTES / sizeof(uint32_t);
            for (size_t index = 0; index < wordCount; ++index) {
                buffer[index] = static_cast<uint32_t>(index) * 0x9E3779B9U ^ 0xA5A55A5AU;
            }
            for (size_t index = 0; index < wordCount; ++index) {
                const uint32_t expected =
                    static_cast<uint32_t>(index) * 0x9E3779B9U ^ 0xA5A55A5AU;
                if (buffer[index] != expected) {
                    result.status = PsramTestStatus::Failed;
                    result.failedWord = index;
                    break;
                }
            }

            if (result.status == PsramTestStatus::Passed) {
                for (size_t index = 0; index < wordCount; ++index) {
                    buffer[index] = ~(static_cast<uint32_t>(index) * 0x9E3779B9U ^ 0xA5A55A5AU);
                }
                for (size_t index = 0; index < wordCount; ++index) {
                    const uint32_t expected =
                        ~(static_cast<uint32_t>(index) * 0x9E3779B9U ^ 0xA5A55A5AU);
                    if (buffer[index] != expected) {
                        result.status = PsramTestStatus::Failed;
                        result.failedWord = index;
                        break;
                    }
                }
            }

            heap_caps_free(buffer);
            return result;
        }

        void drawPsramResult(const PsramTestResult &result)
        {
            gfx->setCursor(8, 94);
            gfx->setTextColor(WHITE, BLACK);
            if (result.status == PsramTestStatus::NotFound) {
                gfx->print("PSRAM NOT FOUND");
                gfx->setCursor(8, 106);
                gfx->print("TEST SKIPPED");
                return;
            }

            gfx->printf("P:%uKB F:%uKB",
                        static_cast<unsigned>(result.totalBytes / 1024U),
                        static_cast<unsigned>(result.freeBytes / 1024U));
            gfx->setCursor(8, 106);
            gfx->setTextColor(result.status == PsramTestStatus::Passed ? GREEN : RED, BLACK);
            switch (result.status) {
                case PsramTestStatus::AllocationFailed:
                    gfx->print("TEST ALLOC FAILED");
                    break;
                case PsramTestStatus::Passed:
                    gfx->print("TEST PASS 16KB");
                    break;
                case PsramTestStatus::Failed:
                    gfx->printf("TEST FAIL @%u",
                                static_cast<unsigned>(result.failedWord * sizeof(uint32_t)));
                    break;
                case PsramTestStatus::NotFound:
                    break;
            }
        }
    }

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

        const PsramTestResult psramResult = testPsram();

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
                    gfx->setCursor(8, 59 + ((index / 4) * 12));
                }
            }
        }
        drawPsramResult(psramResult);
        gfx->setCursor(8, 118);
        gfx->setTextColor(WHITE, BLACK);
        gfx->print("Scan complete");
        gfx->flush();
        delay(5000);
    }
}
