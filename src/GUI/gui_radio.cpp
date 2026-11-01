#include "gui_radio.h"
#include "gui_common.h"
#include "gui_core.h"
#include "mod_radio.h"
#include "mod_sound.h"
#include "mod_buttons.h"
#include <Arduino_GFX_Library.h>


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

    static constexpr unsigned long CLICK_RELEASE_MS = 500UL;

    // No long-press flags here — menu-style short-click behavior only

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
        ModuleRadio::rx.setVolume(currentVolume);
    }

    void applyRadioSettings()
    {
        if (radioModeArk) {
            arkFreq = constrain(arkFreq, 100U, 1150U);
            currentRadioFreq = arkFreq;
            ModuleRadio::setAMBand(100U, 1150U, currentRadioFreq, 10U);
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
        }
    }

    void initializeRadio()
    {
        radioModeArk = true;
        receiverModulationFm = true;
        currentRadioFreq = arkFreq;
        applyRadioSettings();
    }

    void drawRadioScreen()
    {
        drawSubMenuLayout(" RADIO ");

        int startY = 20;
        int centerX = 80;
        gfx->fillRect(4, startY, 152, 92, BLACK);

        gfx->setTextSize(1);
        if (ModuleRadio::isChipResponding()) {
            gfx->setTextColor(GREEN, BLACK);
            gfx->setCursor(4, 18);
            gfx->print("[RAD:OK]");

            gfx->setTextSize(0);
            gfx->setTextColor(CYAN, BLACK);
            gfx->setCursor(58, 18);
            gfx->print(radioModeArk ? "ARK" : "RX-MEDIA");

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
        int textY = startY + 30;
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
        gfx->setCursor(4, startY + 72);
        if (isEditingFreq) {
            gfx->setTextColor(frequencyOutOfRange ? RED : YELLOW, BLACK);
            if (frequencyOutOfRange) {
                gfx->print("OUT OF RANGE; * = tune");
            } else {
                gfx->print("Digits; a = backspace");
            }
        } else {
            gfx->setTextColor(RGB565_DARKGREY, BLACK);
            gfx->setCursor(18, startY + 72);
            if (radioModeArk) {
                gfx->print("BAND: ARK");
            } else {
                gfx->print(receiverModulationFm ? "BAND: RX-MEDIA FM" : "BAND: RX-MEDIA AM");
            }
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
                    ModuleSound::play(ModuleSound::SFX_OK);
                }
            }
            return;
        }

        if (!isEditingFreq) {
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
                ModuleSound::play(ModuleSound::SFX_OK);
                return;
            }

            if (event.type == ModuleButtons::EventType::KEY && event.key == 'b') {
                radioModeArk = false;
                receiverModulationFm = true;
                currentRadioFreq = rxMediaFmFreq;
                applyRadioSettings();
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
                ModuleRadio::stop();
                GUI::resetButtonsAndIgnore(800UL);
                currentState = MAIN_MENU;
                drawMainMenu();
                return;
            }
        }
    }
}
