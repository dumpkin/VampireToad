#ifndef MOD_RADIO_H
#define MOD_RADIO_H

#include <Arduino.h>
#include <SI4735.h> // !!! КРИТИЧНИЙ ФІКС: перенесіть цей інклуд сюди, на самий початок хедера

namespace ModuleRadio {
    static constexpr uint8_t RADIO_RST_PIN = 10;

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
