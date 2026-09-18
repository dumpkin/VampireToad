#include "gui_core.h"
#include "gui_common.h"
#include "gui_radio.h"
#include "gui_compass.h"
#include "gui_radar.h"
#include "gui_wifi.h"
#include "gui_ble.h"
#include "mod_sound.h"
#include "mod_gps.h"
#include "mod_gy91.h"
#include "mod_radio.h"
#include "mod_cyber.h"
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

namespace GUI
{
    MenuState currentState = MAIN_MENU;
    int currentItem = 0;

    // Для хакерських режимів (Wi-Fi та BLE)
    int hackerSubStage = 0;
    int selectedTargetIndex = 0;
    int selectedAttackType = 0;

    const int totalItems = 5;
    const char *menuItems[totalItems] = {
        "1. GPS & RADAR",
        "2. COMPASS & IMU",
        "3. WI-FI ATTACK",
        "4. BLE ATTACK",
        "5. RADIO RECEIVER"};

    unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 180;
    unsigned long lastClockUpdate = 0;

    MenuState getCurrentState() { return currentState; }

    void init()
    {
        pinMode(BTN_UP, INPUT_PULLUP);
        pinMode(BTN_DOWN, INPUT_PULLUP);
        pinMode(BTN_OK, INPUT_PULLUP);
        pinMode(BTN_BACK, INPUT_PULLUP);

        pinMode(1, INPUT);
        analogSetAttenuation(ADC_11db);
    }

    void drawMainMenu()
    {
        gfx->fillScreen(BLACK);
        gfx->drawRect(0, 0, 160, 128, GREEN);
        gfx->drawRect(1, 1, 158, 126, GREEN);

        gfx->fillRect(0, 0, 160, 12, BLUE);
        gfx->setCursor(2, 2);
        gfx->setTextColor(GREEN, BLUE);
        gfx->print("VTOAD ");

        gfx->setTextColor(WHITE, BLUE);
        gfx->print("[SD]");
        if (ModuleGY91::isSensorReady())
        {
            gfx->setTextColor(GREEN, BLUE);
            gfx->print("[GY]");
        }
        else
        {
            gfx->setTextColor(RED, BLUE);
            gfx->print("[ERR]");
        }
        gfx->setTextColor(WHITE, BLUE);
        gfx->print("[RF]");

        for (int i = 0; i < totalItems; i++)
        {
            int yPos = 18 + (i * 18);
            if (i == currentItem)
            {
                gfx->fillRect(4, yPos, 152, 15, GREEN);
                gfx->setTextColor(BLACK, GREEN);
            }
            else
            {
                gfx->setTextColor(GREEN, BLACK);
            }
            gfx->setCursor(8, yPos + 4);
            gfx->setTextSize(1);
            gfx->print(menuItems[i]);
        }

        drawUniversalFooter();
    }

    void checkButtons()
    {
        if (millis() - lastClockUpdate >= 500)
        {
            lastClockUpdate = millis();
            drawUniversalFooter();
        }

        if ((millis() - lastDebounceTime) < debounceDelay)
            return;

        bool up = (digitalRead(BTN_UP) == LOW);
        bool down = (digitalRead(BTN_DOWN) == LOW);
        bool ok = (digitalRead(BTN_OK) == LOW);
        bool back = (digitalRead(BTN_BACK) == LOW);

        if (up || down || ok || back)
        {
            lastDebounceTime = millis();

            // --- ГОЛОВНЕ МЕНЮ ---
            if (currentState == MAIN_MENU)
            {
                if (up)
                {
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                    currentItem = (currentItem + 1) % totalItems;
                    drawMainMenu();
                }
                else if (down)
                {
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                    currentItem = (currentItem - 1 + totalItems) % totalItems;
                    drawMainMenu();
                }
                else if (ok)
                {
                    ModuleSound::play(ModuleSound::SFX_OK);
                    gfx->fillScreen(BLACK);
                    hackerSubStage = 0;
                    selectedTargetIndex = 0;
                    selectedAttackType = 0;

                    if (currentItem == 0)
                    {
                        currentState = SUB_RADAR;
                        drawVisualRadarScreen();
                    }
                    else if (currentItem == 1)
                    {
                        currentState = SUB_COMPASS;
                        drawVisualCompass();
                    }
                    else if (currentItem == 2)
                    {
                        currentState = SUB_WIFI;
                        ModuleCyber::startWiFiScan();
                        drawWiFiScreen();
                    }
                    else if (currentItem == 3)
                    {
                        currentState = SUB_BLE;
                        ModuleCyber::startBLEScan();
                        drawBLEScreen();
                    }
                    else if (currentItem == 4)
                    {
                        currentState = SUB_RADIO;
                        ModuleRadio::init();
                        ModuleRadio::setAMBand(100U, 1150U, 561U, 10U);
                        drawRadioScreen();
                    }
                }
            }
            // --- ХАКЕРСЬКІ РЕЖИМИ (Wi-Fi та BLE) ---
            else if (currentState == SUB_WIFI || currentState == SUB_BLE)
            {
                if (up)
                {
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                    if (hackerSubStage == 0)
                        selectedTargetIndex = (selectedTargetIndex + 1) % 5;
                    else if (hackerSubStage == 1)
                        selectedAttackType = (selectedAttackType - 1 + 2) % 2;
                }
                else if (down)
                {
                    ModuleSound::play(ModuleSound::SFX_CLICK);
                    if (hackerSubStage == 0)
                        selectedTargetIndex = (selectedTargetIndex - 1 + 5) % 5;
                    else if (hackerSubStage == 1)
                        selectedAttackType = (selectedAttackType + 1) % 2;
                }
                else if (ok)
                {
                    ModuleSound::play(ModuleSound::SFX_OK);

                    if (hackerSubStage == 0)
                    {
                        if (currentState == SUB_WIFI)
                            ModuleCyber::stopWiFiScan();
                        else
                            ModuleCyber::stopBLEScan();
                        hackerSubStage = 1;
                    }
                    else if (hackerSubStage == 1)
                    {
                        hackerSubStage = 2;

                        if (currentState == SUB_WIFI)
                        {
                            ModuleCyber::WiFiNetwork targetNet = ModuleCyber::getWiFiNetwork(selectedTargetIndex);
                            if (selectedAttackType == 0)
                            {
                                ModuleCyber::startWiFiDeauth(targetNet.bssid, targetNet.channel);
                            }
                            else
                            {
                                strncpy(ModuleCyber::targetSsidClone, targetNet.ssid, 31);
                                ModuleCyber::targetSsidClone[31] = '\0';
                                ModuleCyber::startWiFiBeaconSpam();
                            }
                        }
                        else if (currentState == SUB_BLE)
                        {
                            ModuleCyber::startBLEJammer();
                        }
                    }
                }
                else if (back)
                {
                    ModuleSound::play(ModuleSound::SFX_BACK);

                    if (hackerSubStage == 2)
                    {
                        if (currentState == SUB_WIFI)
                        {
                            ModuleCyber::stopWiFiAttack();
                            ModuleCyber::targetSsidClone[0] = '\0';
                        }
                        else
                        {
                            ModuleCyber::stopBLEAttack();
                        }
                        hackerSubStage = 1;
                        gfx->fillScreen(BLACK);
                    }
                    else if (hackerSubStage == 1)
                    {
                        hackerSubStage = 0;
                        gfx->fillScreen(BLACK);
                        if (currentState == SUB_WIFI)
                            ModuleCyber::startWiFiScan();
                        else
                            ModuleCyber::startBLEScan();
                    }
                    else
                    {
                        if (currentState == SUB_WIFI)
                            ModuleCyber::stopWiFiScan();
                        else
                            ModuleCyber::stopBLEScan();
                        currentState = MAIN_MENU;
                        drawMainMenu();
                    }
                }
            }
            // --- РАДІО РЕЖИМ ---
            else if (currentState == SUB_RADIO)
            {
                handleRadioButtons(up, down, ok, back);
            }
            // --- ЗВИЧАЙНІ МОДУЛІ (GPS та КОМПАС) ---
            else
            {
                // Added compass mode switching: up/down change mode, back returns to menu
                if (currentState == SUB_COMPASS)
                {
                    if (up)
                    {
                        ModuleSound::play(ModuleSound::SFX_CLICK);
                        GUI::modeNext(); // was modeNext();
                        drawVisualCompass();
                    }
                    else if (down)
                    {
                        ModuleSound::play(ModuleSound::SFX_CLICK);
                        GUI::modePrev(); // was modePrev();
                        drawVisualCompass();
                    }
                    else if (back)
                    {
                        ModuleSound::play(ModuleSound::SFX_BACK);
                        currentState = MAIN_MENU;
                        ModuleGPS::stop();
                        drawMainMenu();
                    }
                }
              else
                {
                    if (back)
                    {
                        ModuleSound::play(ModuleSound::SFX_BACK);
                        currentState = MAIN_MENU;
                        ModuleGPS::stop();
                        drawMainMenu();
                    }
                }
            }
        
        
        }
    
    }
}