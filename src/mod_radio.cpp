#include "mod_radio.h"
#include "mod_gy91.h"
#include <SI4735.h>
#include <Wire.h>
#include <SparkFunSX1509.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;
extern SX1509 io; // Надаємо модулю радіо доступ до SX1509 з main.cpp

// ДОДАТИ ЦЕЙ РЯДОК: пов'язуємо гучність з файлу gui_radio.cpp
namespace GUI {
    extern int currentVolume;
}


namespace ModuleRadio
{
    SI4735 rx;
    bool isRadioReady = false;
    uint32_t activeTuneStep = TUNE_STEP_250;

    bool isChipResponding()
    {
        return isRadioReady;
    }

    void setTuneStep(uint32_t stepKhz)
    {
        activeTuneStep = (stepKhz == TUNE_STEP_500) ? TUNE_STEP_500 : TUNE_STEP_250;
    }

    uint32_t getTuneStep()
    {
        return activeTuneStep;
    }

    void setAMBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz)
    {
        if (!isRadioReady) {
           return;
        }

        if (minKhz < 100U) minKhz = 100U;
        if (maxKhz > 1150U) maxKhz = 1150U;
        if (startKhz < minKhz) startKhz = minKhz;
        if (startKhz > maxKhz) startKhz = maxKhz;

        rx.setAM((uint16_t)minKhz, (uint16_t)maxKhz, (uint16_t)startKhz, (uint16_t)stepKhz);
        rx.setFrequency((uint16_t)startKhz);
    }

    void setFMBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz)
    {
        if (!isRadioReady) {
           return;
        }

        setTuneStep(stepKhz);

        const uint16_t libMin = (uint16_t)(minKhz / 10U);
        const uint16_t libMax = (uint16_t)(maxKhz / 10U);
        const uint16_t libStart = (uint16_t)(startKhz / 10U);
        const uint16_t libStep = (uint16_t)(activeTuneStep / 10U);

        rx.setFM(libMin, libMax, libStart, libStep);
        rx.setFrequency(libStart);
    }

    void setFrequencyBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz)
    {
        if (!isRadioReady) {
           return;
        }

        setFMBand(minKhz, maxKhz, startKhz, stepKhz);
    }

    void init()
    {
        if (!io.begin(0x3E)) {
            Serial.println("[RADIO] SX1509 not found at 0x3E");
            isRadioReady = false;
            return;
        }

        gfx->fillRect(0, 0, 128, 20, RED);
        gfx->setTextColor(BLACK, RED);
        gfx->setTextSize(1);
        gfx->setCursor(4, 4);
        gfx->print("[RADIO] SX1509 OK");
        gfx->flush();

        // 1. БЕЗПЕЧНА ПЕРЕВІРКА АДРЕСИ (Метод бібліотеки pu2clr)
        int16_t detectedAddr = 0;
        if (RADIO_RST_ON_EXPANDER) {
            detectedAddr = rx.getDeviceI2CAddress(RADIO_RST_PIN_EXPANDER);
        } else {
            // Якщо reset на ESP GPIO, підготуємо пін і передамо його бібліотеці
            pinMode(RADIO_RST_GPIO, OUTPUT);
            digitalWrite(RADIO_RST_GPIO, HIGH);
            detectedAddr = rx.getDeviceI2CAddress(RADIO_RST_GPIO);
        }

        if (detectedAddr == 0) {
            gfx->fillRect(0, 20, 128, 20, RED);
            gfx->setTextColor(WHITE, RED);
            gfx->setCursor(4, 24);
            gfx->print("[RADIO] ADDR FAIL");
            gfx->flush();
            isRadioReady = false;
            Serial.println("[RADIO] Si4735 not found on I2C");
            return;
        }

        gfx->fillRect(0, 20, 128, 20, YELLOW);
        gfx->setTextColor(BLACK, YELLOW);
        gfx->setCursor(4, 24);
        gfx->printf("[RADIO] ADDR:%d", detectedAddr);
        gfx->flush();

        // 2. Якщо адреса зафіксована — виконуємо апаратний старт
        if (RADIO_RST_ON_EXPANDER) {
            rx.setup(RADIO_RST_PIN_EXPANDER, 0);
        } else {
            rx.setup(RADIO_RST_GPIO, 0);
        }
        delay(100);

        // 3. Правильне налаштування кварцу 32.768 кГц
        rx.setRefClock(32768);
        rx.setRefClockPrescaler(1);

        gfx->fillRect(0, 40, 128, 20, GREEN);
        gfx->setTextColor(BLACK, GREEN);
        gfx->setCursor(4, 44);
        gfx->print("[RADIO] SETUP OK");
        gfx->flush();

        isRadioReady = true;
        rx.setVolume(GUI::currentVolume);

        gfx->fillRect(0, 60, 128, 20, BLUE);
        gfx->setTextColor(WHITE, BLUE);
        gfx->setCursor(4, 64);
        gfx->print("[RADIO] READY");
        gfx->flush();

        Serial.println("[RADIO] Si4735 initialized");
    }




    void setFrequency(uint32_t freqKhz)
    {
        if (!isRadioReady) {
           return;
        }

        if (freqKhz < 100U) {
           freqKhz = 100U;
        }

        if (freqKhz <= 1150U) {
           if (freqKhz > 1150U) freqKhz = 1150U;
           rx.setAM(100U, 1150U, (uint16_t)freqKhz, 10U);
           return;
        }

        if (freqKhz < 6400U) {
           freqKhz = 6400U;
        }
        if (freqKhz > 108000U) {
           freqKhz = 108000U;
        }

        rx.setFrequency((uint16_t)(freqKhz / 10U));
    }

    void stop()
    {
        if (!isRadioReady) {
           return;
        }

        rx.powerDown();
    }
}
