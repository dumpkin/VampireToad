#include "gui_core.h"
#include "gui_common.h"
#include "gui_radio.h"
#include "gui_navigation.h"
#include "gui_wifi.h"
#include "gui_ble.h"
#include "mod_sound.h"
#include "mod_gps.h"
#include "mod_gy91.h"
#include "mod_radio.h"
#include "mod_cyber.h"
#include "mod_buttons.h"
#include "mod_test.h"
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

    const int totalItems = 4;
    const char *menuItems[totalItems] = {
        "1. GPS-COMPASS",
        "2. WI-FI SCAN",
        "3. BLE SCAN",
        "4. RADIO RECEIVER"};

    unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 300; // increased to reduce touch-like chatter
    unsigned long lastClockUpdate = 0;

    // Скинути стан кнопок і встановити ігнор на ms мілісекунд
    void resetButtonsAndIgnore(unsigned long ms) {
        ModuleButtons::resetAndIgnore(ms);
    }

    MenuState getCurrentState() { return currentState; }

 void init()
{
     ModuleButtons::begin();

     // Нативний аналоговий пін батареї залишається без змін на процесорі
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

static bool hasButton(const ModuleButtons::Event &event, ModuleButtons::Button button)
{
    return (event.buttons & static_cast<uint8_t>(button)) != 0;
}

static void openSelectedMenuItem()
{
    ModuleSound::play(ModuleSound::SFX_OK);
    gfx->fillScreen(BLACK);
    hackerSubStage = 0;
    selectedTargetIndex = 0;
    selectedAttackType = 0;

    if (currentItem == 0) {
        currentState = SUB_NAVIGATION;
        drawNavigationScreen();
    } else if (currentItem == 1) {
        currentState = SUB_WIFI;
        ModuleCyber::startWiFiScan();
        drawWiFiScreen();
    } else if (currentItem == 2) {
        currentState = SUB_BLE;
        ModuleCyber::startBLEScan();
        drawBLEScreen();
    } else if (currentItem == 3) {
        currentState = SUB_RADIO;
        resetButtonsAndIgnore(800UL);
        ModuleRadio::init();
        initializeRadio();
        drawRadioScreen();
    }
}

static void handleEvent(const ModuleButtons::Event &event)
{
    if (event.type == ModuleButtons::EventType::COMBINATION &&
        event.buttons == (ModuleButtons::UP | ModuleButtons::DOWN)) {
        ModuleTest::runI2CScan();
        resetButtonsAndIgnore(300UL);
        return;
    }

    if (currentState == MAIN_MENU) {
        if (event.type != ModuleButtons::EventType::CLICK) return;

        if (hasButton(event, ModuleButtons::UP)) {
            ModuleSound::play(ModuleSound::SFX_CLICK);
            currentItem = (currentItem + 1) % totalItems;
            drawMainMenu();
        } else if (hasButton(event, ModuleButtons::DOWN)) {
            ModuleSound::play(ModuleSound::SFX_CLICK);
            currentItem = (currentItem - 1 + totalItems) % totalItems;
            drawMainMenu();
        } else if (hasButton(event, ModuleButtons::OK)) {
            openSelectedMenuItem();
        }
        return;
    }

    if (currentState == SUB_WIFI || currentState == SUB_BLE) {
        if (event.type != ModuleButtons::EventType::CLICK) return;

        const int deviceCount = currentState == SUB_WIFI
            ? ModuleCyber::getWiFiCount()
            : ModuleCyber::getBLECount();

        if (hasButton(event, ModuleButtons::UP) && deviceCount > 0) {
            selectedTargetIndex = (selectedTargetIndex + 1) % deviceCount;
            ModuleSound::play(ModuleSound::SFX_CLICK);
        } else if (hasButton(event, ModuleButtons::DOWN) && deviceCount > 0) {
            selectedTargetIndex = (selectedTargetIndex - 1 + deviceCount) % deviceCount;
            ModuleSound::play(ModuleSound::SFX_CLICK);
        } else if (hasButton(event, ModuleButtons::BACK)) {
            if (currentState == SUB_WIFI) ModuleCyber::stopWiFiScan();
            else ModuleCyber::stopBLEScan();
            hackerSubStage = 0;
            selectedTargetIndex = 0;
            resetButtonsAndIgnore(300UL);
            currentState = MAIN_MENU;
            drawMainMenu();
            ModuleSound::play(ModuleSound::SFX_BACK);
        }
        return;
    }

    if (currentState == SUB_RADIO) {
        handleRadioButtons(event);
        return;
    }

    if (currentState == SUB_NAVIGATION &&
        event.type == ModuleButtons::EventType::CLICK) {
        if (hasButton(event, ModuleButtons::UP)) {
            navigationNextMetric();
            ModuleSound::play(ModuleSound::SFX_CLICK);
            drawNavigationScreen();
        } else if (hasButton(event, ModuleButtons::DOWN)) {
            navigationPreviousMetric();
            ModuleSound::play(ModuleSound::SFX_CLICK);
            drawNavigationScreen();
        } else if (hasButton(event, ModuleButtons::BACK)) {
            ModuleSound::play(ModuleSound::SFX_BACK);
            resetButtonsAndIgnore(800UL);
            currentState = MAIN_MENU;
            drawMainMenu();
        }
    }
}

void checkButtons()
{
    // 1. Оновлення годинника (кожні 500мс)
    const unsigned long now = millis();
    if (now - lastClockUpdate >= 500)
    {
        lastClockUpdate = now;
        drawUniversalFooter();
    }

    ModuleButtons::update();
    ModuleButtons::Event event;
    while (ModuleButtons::popEvent(event)) {
        handleEvent(event);
    }
}


}