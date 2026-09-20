#ifndef MOD_RADIO_H
#define MOD_RADIO_H

#include <Arduino.h>
#include <SI4735.h>

namespace ModuleRadio {

// Radio reset pin configuration.
static constexpr bool RADIO_RST_ON_EXPANDER = false;
static constexpr uint8_t RADIO_RST_PIN_EXPANDER = 0;
static constexpr uint8_t RADIO_RST_GPIO = 10;


    enum TuneStep : uint32_t {
        TUNE_STEP_250 = 250,
        TUNE_STEP_500 = 500
    };

    extern SI4735 rx;

    void init();
    void setFrequency(uint32_t freqKhz);
    void setAMBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz);
    void setFMBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz);
    void setFrequencyBand(uint32_t minKhz, uint32_t maxKhz, uint32_t startKhz, uint32_t stepKhz);
    void setTuneStep(uint32_t stepKhz);
    uint32_t getTuneStep();
    void stop();
    bool isChipResponding();
}

#endif
