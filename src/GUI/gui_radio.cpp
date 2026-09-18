#include "gui_radio.h"
#include "gui_common.h"
#include "gui_core.h"
#include "mod_radio.h"
#include "mod_sound.h"
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

namespace GUI {
    uint32_t currentRadioFreq = 561U;
    int currentVolume = 45;

    int freqDigits[5] = {0, 5, 6, 1, 0};
    int activeDigitIdx = 0;
    bool isEditingFreq = false;
    bool radioModeArk = true;
    bool receiverModulationFm = true;

    unsigned long lastCursorBlink = 0;
    bool cursorState = true;

    static bool upWasPressed = false;
    static bool downWasPressed = false;
    static unsigned long upPressStart = 0;
    static unsigned long downPressStart = 0;
    static bool upHoldTriggered = false;
    static bool downHoldTriggered = false;

    void applyRadioSettings()
    {
        if (radioModeArk) {
            currentRadioFreq = constrain(currentRadioFreq, 100U, 1150U);
            ModuleRadio::setAMBand(100U, 1150U, currentRadioFreq, 10U);
            return;
        }

        if (receiverModulationFm) {
            currentRadioFreq = constrain(currentRadioFreq, 6400U, 10800U);
            ModuleRadio::setFMBand(6400U, 10800U, currentRadioFreq, 250U);
        } else {
            currentRadioFreq = constrain(currentRadioFreq, 520U, 1710U);
            ModuleRadio::setAMBand(520U, 1710U, currentRadioFreq, 10U);
        }
    }

    void prepareEditDigits()
    {
        if (radioModeArk) {
            activeDigitIdx = 0;
            freqDigits[0] = (currentRadioFreq / 1000) % 10;
            freqDigits[1] = (currentRadioFreq / 100) % 10;
            freqDigits[2] = (currentRadioFreq / 10) % 10;
            freqDigits[3] = currentRadioFreq % 10;
            freqDigits[4] = 0;
        } else {
            activeDigitIdx = 0;
            uint32_t v = currentRadioFreq;
            freqDigits[0] = (v / 10000U) % 10U;
            freqDigits[1] = (v / 1000U) % 10U;
            freqDigits[2] = (v / 100U) % 10U;
            freqDigits[3] = (v / 10U) % 10U;
            freqDigits[4] = v % 10U;
        }
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

            gfx->setTextColor(CYAN, BLACK);
            gfx->setCursor(72, 18);
            gfx->print(radioModeArk ? "ARK" : "RADIO");

            gfx->setTextColor(YELLOW, BLACK);
            gfx->setCursor(104, 18);
            gfx->printf("VOL:%02d", currentVolume);
        } else {
            gfx->setTextColor(RED, BLACK);
            gfx->setCursor(4, 18);
            gfx->print("[RAD:ERR]");
        }

        if (isEditingFreq) {
            if (millis() - lastCursorBlink > 250) {
                lastCursorBlink = millis();
                cursorState = !cursorState;
            }
        }

        gfx->setTextSize(2);
        gfx->setTextColor(GREEN, BLACK);
        int textY = startY + 30;

        if (radioModeArk) {
            int xBase = centerX - 26;
            gfx->setCursor(xBase, textY);
            uint32_t value = isEditingFreq ?
                (uint32_t)freqDigits[0] * 1000U + (uint32_t)freqDigits[1] * 100U +
                (uint32_t)freqDigits[2] * 10U + (uint32_t)freqDigits[3] : currentRadioFreq;
            gfx->printf("%04u", value);
            gfx->setTextSize(1);
            gfx->setCursor(xBase + 52, textY + 11);
            gfx->print("kHz");
            if (isEditingFreq) {
                int cursorX = xBase + activeDigitIdx * 12;
                if (cursorState) gfx->drawFastHLine(cursorX, textY + 17, 8, WHITE);
            }
        } else {
            int xBase = centerX - 36;
            uint32_t value = isEditingFreq
                ? (uint32_t)freqDigits[0] * 10000U + (uint32_t)freqDigits[1] * 1000U +
                  (uint32_t)freqDigits[2] * 100U + (uint32_t)freqDigits[3] * 10U +
                  (uint32_t)freqDigits[4]
                : currentRadioFreq;

            gfx->setCursor(xBase, textY);
            if (receiverModulationFm) {
                uint32_t whole = value / 100U;
                uint32_t frac = value % 100U;
                gfx->setTextColor(GREEN, BLACK);
                gfx->print(whole);
                gfx->setTextColor(YELLOW, BLACK);
                gfx->printf("_%02u", frac);
            } else {
                gfx->setTextColor(GREEN, BLACK);
                gfx->printf("%05u", value);
            }

            gfx->setTextSize(1);
            gfx->setCursor(xBase + 64, textY + 11);
            gfx->print(receiverModulationFm ? "MHz" : "kHz");

            if (isEditingFreq) {
                int cursorX = xBase + activeDigitIdx * 12;
                if (cursorState) gfx->drawFastHLine(cursorX, textY + 17, 8, WHITE);
            }
        }

        gfx->setTextSize(1);
        gfx->setTextColor(RGB565_DARKGREY, BLACK);
        gfx->setCursor(18, startY + 72);
        if (radioModeArk) {
            gfx->print("BAND: ARK");
        } else {
            gfx->print(receiverModulationFm ? "BAND: RADIO FM" : "BAND: RADIO AM");
        }

        drawUniversalFooter();
    }

    void handleRadioButtons(bool up, bool down, bool ok, bool back)
    {
        if (!isEditingFreq) {
            if (up && !upWasPressed) {
                upPressStart = millis();
                upWasPressed = true;
            }
            if (!up && upWasPressed) {
                unsigned long elapsed = millis() - upPressStart;
                if (elapsed < 1500UL) {
                    currentVolume = constrain(currentVolume + 3, 0, 63);
                    ModuleRadio::rx.setVolume(currentVolume);
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                }
                upWasPressed = false;
                upHoldTriggered = false;
                upPressStart = 0;
            }
            if (up && upWasPressed && !upHoldTriggered && (millis() - upPressStart) >= 2000UL) {
                upHoldTriggered = true;
                radioModeArk = !radioModeArk;
                if (radioModeArk) {
                    currentRadioFreq = 561U;
                } else {
                    currentRadioFreq = 10150U;
                    receiverModulationFm = true;
                }
                applyRadioSettings();
                ModuleSound::play(ModuleSound::SFX_OK);
            }

            if (down && !downWasPressed) {
                downPressStart = millis();
                downWasPressed = true;
            }
            if (!down && downWasPressed) {
                unsigned long elapsed = millis() - downPressStart;
                if (elapsed < 1500UL) {
                    currentVolume = constrain(currentVolume - 3, 0, 63);
                    ModuleRadio::rx.setVolume(currentVolume);
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                }
                downWasPressed = false;
                downHoldTriggered = false;
                downPressStart = 0;
            }
            if (down && downWasPressed && !downHoldTriggered && !radioModeArk && (millis() - downPressStart) >= 2000UL) {
                downHoldTriggered = true;
                receiverModulationFm = !receiverModulationFm;
                applyRadioSettings();
                ModuleSound::play(ModuleSound::SFX_OK);
            }

            if (ok) {
                isEditingFreq = true;
                prepareEditDigits();
                ModuleSound::play(ModuleSound::SFX_OK);
            }
            else if (back) {
                ModuleSound::play(ModuleSound::SFX_BACK);
                ModuleRadio::stop();
                currentState = MAIN_MENU;
                drawMainMenu();
            }
        }
        else {
            if (up) {
                if (radioModeArk) {
                    freqDigits[activeDigitIdx] = (freqDigits[activeDigitIdx] + 1) % 10;
                } else {
                    freqDigits[activeDigitIdx] = (freqDigits[activeDigitIdx] + 1) % 10;
                }
                ModuleSound::play(ModuleSound::SFX_CLICK);
            }
            else if (down) {
                if (radioModeArk) {
                    freqDigits[activeDigitIdx] = (freqDigits[activeDigitIdx] - 1 + 10) % 10;
                } else {
                    freqDigits[activeDigitIdx] = (freqDigits[activeDigitIdx] - 1 + 10) % 10;
                }
                ModuleSound::play(ModuleSound::SFX_CLICK);
            }
            else if (ok) {
                if (radioModeArk) {
                    if (activeDigitIdx < 3) {
                        activeDigitIdx++;
                    } else {
                        uint32_t value = static_cast<uint32_t>(freqDigits[0]) * 1000U +
                                         static_cast<uint32_t>(freqDigits[1]) * 100U +
                                         static_cast<uint32_t>(freqDigits[2]) * 10U +
                                         static_cast<uint32_t>(freqDigits[3]);
                        currentRadioFreq = constrain(value, 100U, 1150U);
                        applyRadioSettings();
                        isEditingFreq = false;
                        ModuleSound::play(ModuleSound::SFX_OK);
                    }
                } else {
                    if (activeDigitIdx < 4) {
                        activeDigitIdx++;
                    } else {
                        uint32_t value = static_cast<uint32_t>(freqDigits[0]) * 10000U +
                                         static_cast<uint32_t>(freqDigits[1]) * 1000U +
                                         static_cast<uint32_t>(freqDigits[2]) * 100U +
                                         static_cast<uint32_t>(freqDigits[3]) * 10U +
                                         static_cast<uint32_t>(freqDigits[4]);
                        if (receiverModulationFm) {
                            currentRadioFreq = constrain(value, 6400U, 10800U);
                        } else {
                            currentRadioFreq = constrain(value, 520U, 1710U);
                        }
                        applyRadioSettings();
                        isEditingFreq = false;
                        ModuleSound::play(ModuleSound::SFX_OK);
                    }
                }
            }
            else if (back) {
                ModuleSound::play(ModuleSound::SFX_BACK);
                isEditingFreq = false;
            }
        }
    }
}
