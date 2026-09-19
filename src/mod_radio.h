#ifndef MOD_RADIO_H
#define MOD_RADIO_H

#include <Arduino.h>
#include <SI4735.h> // !!! КРИТИЧНИЙ ФІКС: перенесіть цей інклуд сюди, на самий початок хедера

namespace ModuleRadio {

// Конфігурація для Reset-піна радіомодуля
// Якщо true — RST на SX1509 (expander) фізичний пін RADIO_RST_PIN_EXPANDER
// Якщо false — RST використовуємо прямо на ESP GPIO RADIO_RST_GPIO
static constexpr bool RADIO_RST_ON_EXPANDER = false; // повернути на ESP GPIO (pin 10)
static constexpr uint8_t RADIO_RST_PIN_EXPANDER = 0; 
static constexpr uint8_t RADIO_RST_GPIO = 10; // ESP pin previously used


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
