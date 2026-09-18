#include "mod_radio.h"
#include "mod_gy91.h"
#include <SI4735.h>
#include <Wire.h>

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

    void init() {
        isRadioReady = false;
        activeTuneStep = TUNE_STEP_250;

        Wire.begin(ModuleGY91::GY91_SDA_PIN, ModuleGY91::GY91_SCL_PIN, 400000U);
        delay(50);

        int16_t detectedAddr = rx.getDeviceI2CAddress(RADIO_RST_PIN);
        Serial.printf("[RAD] detected I2C addr: 0x%02X\n", detectedAddr);

        if (detectedAddr == 0) {
           isRadioReady = false;
           return;
        }

        rx.setup(RADIO_RST_PIN, 0);
        delay(100);

        rx.setRefClock(32768);
        rx.setRefClockPrescaler(1);
        rx.setVolume(63);
        rx.setAudioMute(false);

        rx.setAM(100U, 1150U, 561U, 10U);
        isRadioReady = true;
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
