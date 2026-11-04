#include "gui_radio.h"
#include "gui_common.h"
#include "gui_core.h"
#include "mod_radio.h"
#include "mod_sound.h"
#include "mod_buttons.h"
#include <Arduino_GFX_Library.h>
#include <cmath>
#include <cstring>


extern Arduino_GFX *gfx;

namespace GUI {
    uint32_t currentRadioFreq = 561U;
    uint32_t arkFreq = 561U;
    uint32_t rxMediaFmFreq = 10150U;
    uint32_t rxMediaAmFreq = 1000U;
    int currentVolume = 45;
    static constexpr int RADIO_VOLUME_STEP = 5;

    uint32_t enteredFrequency = 0;
    uint8_t frequencyCursor = 0;
    bool isEditingFreq = false;
    bool radioModeArk = true;
    bool receiverModulationFm = true;
    bool waterfallActive = false;
    bool audioAnalyzerActive = true;

    static constexpr int WATERFALL_X = 2;
    static constexpr int WATERFALL_Y = 52;
    static constexpr int WATERFALL_WIDTH = 156;
    static constexpr int WATERFALL_HEIGHT = 62;
    static constexpr unsigned long WATERFALL_SAMPLE_INTERVAL_MS = 20UL;
    static constexpr uint8_t WATERFALL_PIXELS_PER_STEP = 4;
    static constexpr uint8_t WATERFALL_STEPS =
        WATERFALL_WIDTH / WATERFALL_PIXELS_PER_STEP;
    static constexpr int8_t WATERFALL_CENTER_STEP = WATERFALL_STEPS / 2;
    static uint16_t waterfallPixels[WATERFALL_WIDTH * WATERFALL_HEIGHT];
    static uint16_t waterfallSpectrumLine[WATERFALL_WIDTH];
    static uint8_t waterfallScanStep = 0;
    static unsigned long lastWaterfallSampleAt = 0;
    static bool waterfallNeedsClear = true;

    static constexpr uint8_t AUDIO_ADC_PIN = 3;
    static constexpr uint16_t AUDIO_FFT_SIZE = 128;
    static constexpr uint32_t AUDIO_SAMPLE_RATE_HZ = 8000;
    static constexpr uint32_t AUDIO_SAMPLE_PERIOD_US =
        1000000UL / AUDIO_SAMPLE_RATE_HZ;
    static constexpr int AUDIO_WAVE_HEIGHT = 29;
    static constexpr int AUDIO_WAVE_CENTER_Y =
        WATERFALL_Y + AUDIO_WAVE_HEIGHT / 2;
    static constexpr int AUDIO_WAVE_AMPLITUDE =
        (AUDIO_WAVE_HEIGHT - 2) / 2;
    static constexpr int AUDIO_HISTORY_Y = WATERFALL_Y + AUDIO_WAVE_HEIGHT + 2;
    static constexpr int AUDIO_HISTORY_HEIGHT =
        WATERFALL_HEIGHT - AUDIO_WAVE_HEIGHT - 2;
    static constexpr uint8_t AUDIO_SPECTRUM_LEVELS = 30;
    static constexpr unsigned long AUDIO_REFRESH_INTERVAL_MS = 40UL;
    static uint16_t audioSamples[AUDIO_FFT_SIZE];
    static float audioReal[AUDIO_FFT_SIZE];
    static float audioImaginary[AUDIO_FFT_SIZE];
    static float audioSampleMean = 0.0f;
    static float audioWavePeak = 0.0f;
    static uint16_t audioSpectrumHistory[WATERFALL_WIDTH * AUDIO_HISTORY_HEIGHT];
    static uint16_t audioSpectrumLine[WATERFALL_WIDTH];
    static unsigned long lastAudioRefreshAt = 0;
    static bool audioSpectrumNeedsClear = true;

    uint16_t audioSpectrumColor(uint8_t level)
    {
        level = constrain(level, 0, AUDIO_SPECTRUM_LEVELS);
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(level) * 31U) / AUDIO_SPECTRUM_LEVELS);
    }

    void resetAudioSpectrum()
    {
        memset(audioSpectrumHistory, 0, sizeof(audioSpectrumHistory));
        memset(audioSpectrumLine, 0, sizeof(audioSpectrumLine));
        lastAudioRefreshAt = 0;
        audioSpectrumNeedsClear = true;
    }

    void drawAudioAnalyzerGrid()
    {
        gfx->drawFastHLine(WATERFALL_X, WATERFALL_Y + AUDIO_WAVE_HEIGHT,
                           WATERFALL_WIDTH, DARKGREEN);
    }

    void captureAudioSamples()
    {
        uint32_t nextSampleAt = micros();
        for (uint16_t index = 0; index < AUDIO_FFT_SIZE; ++index) {
            if (index > 0) {
                nextSampleAt += AUDIO_SAMPLE_PERIOD_US;
                while (static_cast<int32_t>(micros() - nextSampleAt) < 0) {
                }
            }
            audioSamples[index] = static_cast<uint16_t>(analogRead(AUDIO_ADC_PIN));
        }
    }

    void calculateAudioSpectrum()
    {
        audioSampleMean = 0.0f;
        for (uint16_t index = 0; index < AUDIO_FFT_SIZE; ++index) {
            audioSampleMean += audioSamples[index];
        }
        audioSampleMean /= AUDIO_FFT_SIZE;
        audioWavePeak = 0.0f;

        for (uint16_t index = 0; index < AUDIO_FFT_SIZE; ++index) {
            const float centeredSample = audioSamples[index] - audioSampleMean;
            if (fabsf(centeredSample) > audioWavePeak) {
                audioWavePeak = fabsf(centeredSample);
            }
            const float window = 0.5f -
                0.5f * cosf(2.0f * PI * index / (AUDIO_FFT_SIZE - 1));
            audioReal[index] = centeredSample * window;
            audioImaginary[index] = 0.0f;
        }

        uint16_t reversedIndex = 0;
        for (uint16_t index = 1; index < AUDIO_FFT_SIZE; ++index) {
            uint16_t bit = AUDIO_FFT_SIZE >> 1;
            while (reversedIndex & bit) {
                reversedIndex ^= bit;
                bit >>= 1;
            }
            reversedIndex ^= bit;
            if (index < reversedIndex) {
                const float real = audioReal[index];
                audioReal[index] = audioReal[reversedIndex];
                audioReal[reversedIndex] = real;
                const float imaginary = audioImaginary[index];
                audioImaginary[index] = audioImaginary[reversedIndex];
                audioImaginary[reversedIndex] = imaginary;
            }
        }

        for (uint16_t length = 2; length <= AUDIO_FFT_SIZE; length <<= 1) {
            const float angle = -2.0f * PI / length;
            const float stepReal = cosf(angle);
            const float stepImaginary = sinf(angle);
            const uint16_t halfLength = length >> 1;
            for (uint16_t start = 0; start < AUDIO_FFT_SIZE; start += length) {
                float twiddleReal = 1.0f;
                float twiddleImaginary = 0.0f;
                for (uint16_t offset = 0; offset < halfLength; ++offset) {
                    const uint16_t even = start + offset;
                    const uint16_t odd = even + halfLength;
                    const float oddReal = audioReal[odd] * twiddleReal -
                                          audioImaginary[odd] * twiddleImaginary;
                    const float oddImaginary = audioReal[odd] * twiddleImaginary +
                                               audioImaginary[odd] * twiddleReal;
                    audioReal[odd] = audioReal[even] - oddReal;
                    audioImaginary[odd] = audioImaginary[even] - oddImaginary;
                    audioReal[even] += oddReal;
                    audioImaginary[even] += oddImaginary;

                    const float nextTwiddleReal =
                        twiddleReal * stepReal - twiddleImaginary * stepImaginary;
                    twiddleImaginary =
                        twiddleReal * stepImaginary + twiddleImaginary * stepReal;
                    twiddleReal = nextTwiddleReal;
                }
            }
        }
    }

    void drawAudioSpectrum()
    {
        if (!audioAnalyzerActive) return;

        if (audioSpectrumNeedsClear) {
            gfx->fillRect(WATERFALL_X, WATERFALL_Y,
                          WATERFALL_WIDTH, WATERFALL_HEIGHT, BLACK);
            drawAudioAnalyzerGrid();
            audioSpectrumNeedsClear = false;
        }

        const unsigned long now = millis();
        if (now - lastAudioRefreshAt < AUDIO_REFRESH_INTERVAL_MS) return;
        lastAudioRefreshAt = now;
        captureAudioSamples();
        calculateAudioSpectrum();

        gfx->fillRect(WATERFALL_X, WATERFALL_Y,
                      WATERFALL_WIDTH, AUDIO_WAVE_HEIGHT, BLACK);
        drawAudioAnalyzerGrid();

        for (uint16_t index = 1; index < AUDIO_FFT_SIZE; ++index) {
            const uint16_t previousIndex = index - 1;
            const float waveScale = audioWavePeak > 0.0f
                ? AUDIO_WAVE_AMPLITUDE / audioWavePeak
                : 0.0f;
            const int previousY = AUDIO_WAVE_CENTER_Y -
                static_cast<int>((audioSamples[previousIndex] - audioSampleMean) *
                                 waveScale);
            const int y = AUDIO_WAVE_CENTER_Y -
                static_cast<int>((audioSamples[index] - audioSampleMean) *
                                 waveScale);
            gfx->drawLine(
                WATERFALL_X + static_cast<int>(
                    static_cast<uint32_t>(previousIndex) * (WATERFALL_WIDTH - 1) /
                    (AUDIO_FFT_SIZE - 1)),
                constrain(previousY, WATERFALL_Y + 1,
                          WATERFALL_Y + AUDIO_WAVE_HEIGHT - 2),
                WATERFALL_X + static_cast<int>(
                    static_cast<uint32_t>(index) * (WATERFALL_WIDTH - 1) /
                    (AUDIO_FFT_SIZE - 1)),
                constrain(y, WATERFALL_Y + 1,
                          WATERFALL_Y + AUDIO_WAVE_HEIGHT - 2),
                CYAN);
        }

        for (uint16_t x = 0; x < WATERFALL_WIDTH; ++x) {
            const uint16_t bin = 1 +
                static_cast<uint32_t>(x) * 63 / WATERFALL_WIDTH;
            const float magnitude = sqrtf(audioReal[bin] * audioReal[bin] +
                                          audioImaginary[bin] * audioImaginary[bin]) /
                                    (AUDIO_FFT_SIZE / 4.0f);
            const float normalized =
                2.0f * log10f(magnitude + 1.0f) / log10f(2049.0f);
            const uint8_t level = static_cast<uint8_t>(
                constrain(static_cast<int>(normalized * AUDIO_SPECTRUM_LEVELS),
                          0, AUDIO_SPECTRUM_LEVELS));
            audioSpectrumLine[x] = audioSpectrumColor(level);
        }
        memmove(audioSpectrumHistory + WATERFALL_WIDTH,
                audioSpectrumHistory,
                sizeof(audioSpectrumHistory) -
                    WATERFALL_WIDTH * sizeof(audioSpectrumHistory[0]));
        memcpy(audioSpectrumHistory, audioSpectrumLine,
               sizeof(audioSpectrumLine));
        gfx->draw16bitRGBBitmap(WATERFALL_X, AUDIO_HISTORY_Y,
                                audioSpectrumHistory, WATERFALL_WIDTH,
                                AUDIO_HISTORY_HEIGHT);
        drawAudioAnalyzerGrid();
    }

    void resetWaterfall()
    {
        memset(waterfallPixels, 0, sizeof(waterfallPixels));
        memset(waterfallSpectrumLine, 0, sizeof(waterfallSpectrumLine));
        waterfallScanStep = 0;
        lastWaterfallSampleAt = 0;
        waterfallNeedsClear = true;
    }

    uint16_t waterfallColor(uint8_t rssi)
    {
        if (rssi < 12) return RGB565_DARKGREY;
        if (rssi < 24) return BLUE;
        if (rssi < 38) return BLUE;
        if (rssi < 52) return CYAN;
        if (rssi < 68) return GREEN;
        if (rssi < 86) return YELLOW;
        return RED;
    }

    void drawWaterfallGrid()
    {
        for (int x = 1; x < WATERFALL_WIDTH; x += 20) {
            gfx->drawFastVLine(WATERFALL_X + x, WATERFALL_Y,
                               WATERFALL_HEIGHT, RGB565_DARKGREY);
        }
        for (int y = 10; y < WATERFALL_HEIGHT; y += 10) {
            gfx->drawFastHLine(WATERFALL_X, WATERFALL_Y + y,
                               WATERFALL_WIDTH, RGB565_DARKGREY);
        }
    }

    void drawWaterfall()
    {
        if (waterfallNeedsClear) {
            gfx->fillRect(WATERFALL_X, WATERFALL_Y,
                          WATERFALL_WIDTH, WATERFALL_HEIGHT, BLACK);
            drawWaterfallGrid();
            waterfallNeedsClear = false;
        }
        if (!waterfallActive) return;
        if (!ModuleRadio::isChipResponding()) return;

        const unsigned long now = millis();
        if (now - lastWaterfallSampleAt < WATERFALL_SAMPLE_INTERVAL_MS) return;
        lastWaterfallSampleAt = now;

        const uint32_t scanStep =
            (!radioModeArk && receiverModulationFm) ? 10U : 1U;
        int32_t frequency = static_cast<int32_t>(currentRadioFreq) +
            (static_cast<int32_t>(waterfallScanStep) - WATERFALL_CENTER_STEP) *
                static_cast<int32_t>(scanStep);
        const int32_t minimumFrequency = radioModeArk
            ? 100
            : (receiverModulationFm ? 6400 : 520);
        const int32_t maximumFrequency = radioModeArk
            ? 1150
            : (receiverModulationFm ? 10800 : 30000);
        frequency = constrain(frequency, minimumFrequency, maximumFrequency);
        ModuleRadio::rx.setFrequency(static_cast<uint16_t>(frequency));
        delay(10);
        ModuleRadio::rx.getCurrentReceivedSignalQuality();
        const uint16_t pixelStart =
            waterfallScanStep * WATERFALL_PIXELS_PER_STEP;
        const uint16_t color =
            waterfallColor(ModuleRadio::rx.getCurrentRSSI());
        for (uint8_t pixel = 0; pixel < WATERFALL_PIXELS_PER_STEP; ++pixel) {
            waterfallSpectrumLine[pixelStart + pixel] = color;
        }
        gfx->drawFastHLine(WATERFALL_X + pixelStart,
                           WATERFALL_Y + WATERFALL_HEIGHT - 1,
                           WATERFALL_PIXELS_PER_STEP, color);

        ++waterfallScanStep;
        if (waterfallScanStep >= WATERFALL_STEPS) {
            ModuleRadio::rx.setFrequency(static_cast<uint16_t>(currentRadioFreq));
            memmove(waterfallPixels,
                    waterfallPixels + WATERFALL_WIDTH,
                    sizeof(waterfallPixels) -
                        WATERFALL_WIDTH * sizeof(waterfallPixels[0]));
            memcpy(waterfallPixels +
                       (WATERFALL_HEIGHT - 1) * WATERFALL_WIDTH,
                   waterfallSpectrumLine, sizeof(waterfallSpectrumLine));
            gfx->draw16bitRGBBitmap(WATERFALL_X, WATERFALL_Y, waterfallPixels,
                                    WATERFALL_WIDTH, WATERFALL_HEIGHT);
            drawWaterfallGrid();
            memset(waterfallSpectrumLine, 0, sizeof(waterfallSpectrumLine));
            waterfallScanStep = 0;
        }
    }

    uint8_t frequencyDigitCount()
    {
        return radioModeArk ? 4 : 5;
    }

    uint32_t frequencyDigitPlace(uint8_t cursor)
    {
        uint32_t place = 1;
        for (uint8_t index = cursor + 1; index < frequencyDigitCount(); ++index) {
            place *= 10U;
        }
        return place;
    }

    bool isFrequencyInRange(uint32_t frequency)
    {
        if (radioModeArk) return frequency >= 100U && frequency <= 1150U;
        if (receiverModulationFm) return frequency >= 6400U && frequency <= 10800U;
        return frequency >= 520U && frequency <= 30000U;
    }

    void applyRadioVolume()
    {
        currentVolume = constrain(currentVolume, 0, 63);
        if (ModuleRadio::isChipResponding()) {
            ModuleRadio::rx.setVolume(waterfallActive ? 0 : currentVolume);
        }
    }

    void applyRadioSettings()
    {
        if (radioModeArk) {
            arkFreq = constrain(arkFreq, 100U, 1150U);
            currentRadioFreq = arkFreq;
            ModuleRadio::setAMBand(100U, 1150U, currentRadioFreq, 10U);
            if (ModuleRadio::isChipResponding()) {
                ModuleRadio::rx.setBandwidth(0, 0);
            }
            applyRadioVolume();
            return;
        }

        if (receiverModulationFm) {
            // rxMediaFmFreq stored as units of 0.01 MHz (e.g. 10150 -> 101.50 MHz)
            rxMediaFmFreq = constrain(rxMediaFmFreq, 6400U, 10800U);
            currentRadioFreq = rxMediaFmFreq;
            // ModuleRadio::setFMBand expects kHz values; scale: x10 of our 0.01MHz units -> kHz
            ModuleRadio::setFMBand(64000U, 108000U, currentRadioFreq * 10U, 250U);
        } else {
            // rxMediaAmFreq stored in kHz
            rxMediaAmFreq = constrain(rxMediaAmFreq, 520U, 30000U);
            currentRadioFreq = rxMediaAmFreq;
            ModuleRadio::setAMBand(520U, 30000U, currentRadioFreq, 10U);
            if (ModuleRadio::isChipResponding()) {
                ModuleRadio::rx.setBandwidth(0, 0);
            }
        }
        applyRadioVolume();
    }

    void initializeRadio()
    {
        radioModeArk = true;
        receiverModulationFm = true;
        waterfallActive = false;
        audioAnalyzerActive = true;
        pinMode(AUDIO_ADC_PIN, INPUT);
        analogReadResolution(12);
        analogSetPinAttenuation(AUDIO_ADC_PIN, ADC_11db);
        currentRadioFreq = arkFreq;
        applyRadioSettings();
        resetWaterfall();
        resetAudioSpectrum();
    }

    void drawRadioScreen()
    {
        drawSubMenuLayout(" RADIO ");

        int centerX = 80;
        gfx->fillRect(2, 16, 156, WATERFALL_Y - 16, BLACK);
        gfx->drawRect(WATERFALL_X - 1, WATERFALL_Y - 1,
                      WATERFALL_WIDTH + 2, WATERFALL_HEIGHT + 2, DARKGREEN);

        gfx->setTextSize(1);
        if (ModuleRadio::isChipResponding()) {
            gfx->setTextColor(GREEN, BLACK);
            gfx->setCursor(4, 18);
            gfx->print("[RAD:OK]");

            gfx->setTextSize(0);
            gfx->setTextColor(CYAN, BLACK);
            gfx->setCursor(58, 18);
            gfx->print(radioModeArk ? "АРК" : (receiverModulationFm ? "FM" : "АМ"));

            gfx->setTextSize(1);
            gfx->setTextColor(YELLOW, BLACK);
            gfx->setCursor(104, 18);
            gfx->printf("VOL:%02d", currentVolume);
        } else {
            gfx->setTextColor(RED, BLACK);
            gfx->setCursor(4, 18);
            gfx->print("[RAD:ERR]");
        }

        gfx->setTextSize(2);
        gfx->setTextColor(GREEN, BLACK);
        int textY = 25;
        const uint32_t displayedFrequency = isEditingFreq ? enteredFrequency : currentRadioFreq;
        const bool frequencyOutOfRange = isEditingFreq &&
                                         !isFrequencyInRange(displayedFrequency);

        if (radioModeArk) {
            int xBase = centerX - 26;
            char digits[5];
            snprintf(digits, sizeof(digits), "%04u", static_cast<unsigned>(displayedFrequency));
            for (uint8_t index = 0; index < 4; ++index) {
                gfx->setTextColor(frequencyOutOfRange ? RED : GREEN, BLACK);
                gfx->setCursor(xBase + index * 12, textY);
                gfx->print(digits[index]);
                if (isEditingFreq && index == frequencyCursor) {
                    gfx->drawFastHLine(xBase + index * 12, textY + 16, 10, YELLOW);
                }
            }
            gfx->setTextSize(1);
            gfx->setCursor(xBase + 52, textY + 11);
            gfx->print("kHz");
        } else {
            int xBase = centerX - 36;
            char digits[6];
            snprintf(digits, sizeof(digits), "%05u", static_cast<unsigned>(displayedFrequency));
            for (uint8_t index = 0; index < 5; ++index) {
                uint16_t color = frequencyOutOfRange ? RED : GREEN;
                if (!frequencyOutOfRange && receiverModulationFm && index >= 3) {
                    color = YELLOW;
                }
                gfx->setTextColor(color, BLACK);
                gfx->setCursor(xBase + index * 12, textY);
                gfx->print(digits[index]);
                if (isEditingFreq && index == frequencyCursor) {
                    gfx->drawFastHLine(xBase + index * 12, textY + 16, 10, YELLOW);
                }
            }

            gfx->setTextSize(1);
            gfx->setCursor(xBase + 64, textY + 11);
            gfx->print(receiverModulationFm ? "MHz" : "kHz");

        }

        gfx->setTextSize(1);
        gfx->setCursor(4, 45);
        if (isEditingFreq) {
            gfx->setTextColor(frequencyOutOfRange ? RED : YELLOW, BLACK);
            if (frequencyOutOfRange) {
                gfx->print("OUT OF RANGE");
            } else {
                gfx->print("Digits; a = back");
            }
        } else {
            gfx->setTextColor(audioAnalyzerActive ? CYAN :
                              (waterfallActive ? GREEN : RGB565_DARKGREY), BLACK);
            if (audioAnalyzerActive) {
                gfx->print("AUDIO FFT (1=OFF)");
            } else {
                gfx->print(waterfallActive ? "WF ON  (0=OFF)" : "WF OFF (0=ON)");
            }
        }

        if (audioAnalyzerActive) {
            drawAudioSpectrum();
        } else {
            drawWaterfall();
        }
        drawUniversalFooter();
    }

    void handleRadioButtons(const ModuleButtons::Event &event)
    {
        const bool click = event.type == ModuleButtons::EventType::CLICK;
        const bool upEvent = click && event.buttons == ModuleButtons::UP;
        const bool downEvent = click && event.buttons == ModuleButtons::DOWN;
        const bool okEvent = click && event.buttons == ModuleButtons::OK;
        const bool backEvent = click && event.buttons == ModuleButtons::BACK;

        if (isEditingFreq && event.type == ModuleButtons::EventType::KEY) {
            if (event.key == 'a') {
                if (frequencyCursor > 0) {
                    --frequencyCursor;
                    const uint32_t place = frequencyDigitPlace(frequencyCursor);
                    const uint32_t digit = (enteredFrequency / place) % 10U;
                    enteredFrequency -= digit * place;
                    ModuleSound::play(ModuleSound::SFX_BACK);
                }
            } else if (event.key >= '0' && event.key <= '9' &&
                       frequencyCursor < frequencyDigitCount()) {
                const uint32_t place = frequencyDigitPlace(frequencyCursor);
                const uint32_t previousDigit = (enteredFrequency / place) % 10U;
                enteredFrequency = enteredFrequency - previousDigit * place +
                                   static_cast<uint32_t>(event.key - '0') * place;
                ++frequencyCursor;
                ModuleSound::play(ModuleSound::SFX_CLICK);
            }
            return;
        }

        if (isEditingFreq) {
            if (backEvent) {
                isEditingFreq = false;
                ModuleSound::play(ModuleSound::SFX_BACK);
            } else if (okEvent) {
                if (isFrequencyInRange(enteredFrequency)) {
                    currentRadioFreq = enteredFrequency;
                    if (radioModeArk) arkFreq = currentRadioFreq;
                    else if (receiverModulationFm) rxMediaFmFreq = currentRadioFreq;
                    else rxMediaAmFreq = currentRadioFreq;
                    isEditingFreq = false;
                    applyRadioSettings();
                    resetWaterfall();
                    ModuleSound::play(ModuleSound::SFX_OK);
                }
            }
            return;
        }

        if (!isEditingFreq) {
            if (event.type == ModuleButtons::EventType::KEY && event.key == '1') {
                audioAnalyzerActive = true;
                waterfallActive = false;
                resetWaterfall();
                resetAudioSpectrum();
                if (ModuleRadio::isChipResponding()) {
                    ModuleRadio::rx.setFrequency(
                        static_cast<uint16_t>(currentRadioFreq));
                }
                applyRadioVolume();
                ModuleSound::play(ModuleSound::SFX_CLICK);
                return;
            }

            if (event.type == ModuleButtons::EventType::KEY && event.key == '0') {
                waterfallActive = !waterfallActive;
                audioAnalyzerActive = !waterfallActive;
                resetAudioSpectrum();
                if (waterfallActive) {
                    resetWaterfall();
                    applyRadioVolume();
                } else {
                    resetWaterfall();
                    if (ModuleRadio::isChipResponding()) {
                        ModuleRadio::rx.setFrequency(
                            static_cast<uint16_t>(currentRadioFreq));
                    }
                    applyRadioVolume();
                }
                ModuleSound::play(ModuleSound::SFX_CLICK);
                return;
            }

            if (event.type == ModuleButtons::EventType::KEY && event.key == 'a') {
                if (radioModeArk) {
                    radioModeArk = false;
                    receiverModulationFm = false;
                    currentRadioFreq = rxMediaAmFreq;
                } else {
                    radioModeArk = true;
                    currentRadioFreq = arkFreq;
                }
                applyRadioSettings();
                resetWaterfall();
                ModuleSound::play(ModuleSound::SFX_OK);
                return;
            }

            if (event.type == ModuleButtons::EventType::KEY && event.key == 'b') {
                radioModeArk = false;
                receiverModulationFm = true;
                currentRadioFreq = rxMediaFmFreq;
                applyRadioSettings();
                resetWaterfall();
                ModuleSound::play(ModuleSound::SFX_OK);
                return;
            }

            // Single short clicks
            if (upEvent) {
                currentVolume = constrain(currentVolume - RADIO_VOLUME_STEP, 0, 63);
                applyRadioVolume();
                ModuleSound::play(ModuleSound::SFX_CLICK);
                return;
            }

            if (downEvent) {
                currentVolume = constrain(currentVolume + RADIO_VOLUME_STEP, 0, 63);
                applyRadioVolume();
                ModuleSound::play(ModuleSound::SFX_CLICK);
                return;
            }

            if (okEvent) {
                isEditingFreq = true;
                enteredFrequency = currentRadioFreq;
                frequencyCursor = 0;
                ModuleSound::play(ModuleSound::SFX_OK);
                return;
            }

            if (backEvent) {
                ModuleSound::play(ModuleSound::SFX_BACK);
                waterfallActive = false;
                audioAnalyzerActive = false;
                ModuleRadio::stop();
                GUI::resetButtonsAndIgnore(800UL);
                currentState = MAIN_MENU;
                drawMainMenu();
                return;
            }
        }
    }
}
