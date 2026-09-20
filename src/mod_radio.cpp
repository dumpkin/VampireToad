#include "mod_radio.h"
#include "mod_gy91.h"
#include "mod_test.h"
#include <SI4735.h>
#include <Wire.h>
#include <SparkFunSX1509.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;
extern SX1509 io;

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
        // SX1509 is already initialized in main setup.
        int16_t detectedAddr = 0;
        if (RADIO_RST_ON_EXPANDER) {
            detectedAddr = rx.getDeviceI2CAddress(RADIO_RST_PIN_EXPANDER);
        } else {
            pinMode(RADIO_RST_GPIO, OUTPUT);
            digitalWrite(RADIO_RST_GPIO, HIGH);
            detectedAddr = rx.getDeviceI2CAddress(RADIO_RST_GPIO);
        }

        if (detectedAddr == 0) {
            isRadioReady = false;
            Serial.println("[RADIO] Si4735 not found on I2C");
            return;
        }

        // External RCLK reference.
        rx.setRefClock(32768);
        rx.setRefClockPrescaler(1);

        if (RADIO_RST_ON_EXPANDER) {
            rx.setup(RADIO_RST_PIN_EXPANDER, 0, 1, SI473X_ANALOG_AUDIO, XOSCEN_RCLK, 0);
        } else {
            rx.setup(RADIO_RST_GPIO, 0, 1, SI473X_ANALOG_AUDIO, XOSCEN_RCLK, 0);
        }
        delay(100);

        isRadioReady = true;
        rx.setVolume(GUI::currentVolume);
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
