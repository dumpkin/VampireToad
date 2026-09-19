#include "gui_core.h"
#include "gui_common.h"
#include "gui_radio.h"
#include "gui_compass.h"
#include "gui_radar.h"
#include "gui_wifi.h"
#include "gui_ble.h"
#include "gui_test.h"
#include "mod_sound.h"
#include "mod_gps.h"
#include "mod_gy91.h"
#include "mod_radio.h"
#include "mod_cyber.h"
#include <Arduino_GFX_Library.h>
#include <SparkFunSX1509.h> // ДОДАТИ ЦЕЙ РЯДОК НА ПОЧАТОК ФАЙЛУ


extern Arduino_GFX *gfx;
extern SX1509 io; // Надаємо ядру графіки доступ до об'єкта з main.cpp

namespace GUI {
    void runGuiTest();
}

extern SX1509 io;

// ДОДАТИ ЦІ РЯДКИ: зв'язуємо змінні з gui_radio.cpp
namespace GUI {
    extern uint32_t currentRadioFreq;
    extern bool radioModeArk;
    extern bool receiverModulationFm;
    void applyRadioSettings();
}


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
    const unsigned long debounceDelay = 300; // increased to reduce touch-like chatter
    unsigned long lastClockUpdate = 0;

    // Ігнорувати кнопки до цього часу (ms) — використовуємо при переході/ініціалізації
    static unsigned long buttonsIgnoreUntil = 0;

    // forward declarations for button state arrays used by resetButtonsAndIgnore()
    static unsigned long buttonPressStart[16];
    static bool buttonWasPressed[16];
    static bool holdTriggered[16];
    static bool upDownTestLatched = false;

    // Скинути стан кнопок і встановити ігнор на ms мілісекунд
    void resetButtonsAndIgnore(unsigned long ms) {
        for (int i = 0; i < 16; ++i) { buttonWasPressed[i] = false; holdTriggered[i] = false; buttonPressStart[i] = 0; }
        buttonsIgnoreUntil = millis() + ms;
        // скидати джерела переривань, щоб уникнути накопичення
        io.interruptSource();
        // відновити INPUT_PULLUP та апаратний дебаунс/переривання на випадок, якщо expander був скинутий
        io.pinMode(BTN_UP, INPUT_PULLUP);
        io.pinMode(BTN_DOWN, INPUT_PULLUP);
        io.pinMode(BTN_OK, INPUT_PULLUP);
        io.pinMode(BTN_BACK, INPUT_PULLUP);
        io.debouncePin(BTN_UP);
        io.debouncePin(BTN_DOWN);
        io.debouncePin(BTN_OK);
        io.debouncePin(BTN_BACK);
        io.enableInterrupt(BTN_UP, CHANGE);
        io.enableInterrupt(BTN_DOWN, CHANGE);
        io.enableInterrupt(BTN_OK, CHANGE);
        io.enableInterrupt(BTN_BACK, CHANGE);
    }

    MenuState getCurrentState() { return currentState; }

 void init()
{
    // Налаштовуємо піни 1-4 на INPUT із вбудованою підтяжкою до 3.3V
    io.pinMode(BTN_UP, INPUT_PULLUP);
    io.pinMode(BTN_DOWN, INPUT_PULLUP);
    io.pinMode(BTN_OK, INPUT_PULLUP);
    io.pinMode(BTN_BACK, INPUT_PULLUP);

     // Додатково встановлюємо підтяжку на пін переривання ESP (на всякий випадок)
     pinMode(SX1509_INT_PIN, INPUT_PULLUP);

     // Активуємо апаратний антибрязк чіпа саме на цих чотирьох каналах
     io.debouncePin(BTN_UP);
     io.debouncePin(BTN_DOWN);
     io.debouncePin(BTN_OK);
     io.debouncePin(BTN_BACK);

     // Налаштовуємо генерацію переривання при будь-якій зміні рівня (натиснули/відпустили)
     io.enableInterrupt(BTN_UP, CHANGE);
     io.enableInterrupt(BTN_DOWN, CHANGE);
     io.enableInterrupt(BTN_OK, CHANGE);
     io.enableInterrupt(BTN_BACK, CHANGE);

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

// Зберігаємо змінні для точного відстеження стану кожної кнопки
// ВИПРАВЛЕНО: Розмір 16 повністю покриває всі можливі піни розширювача портів
static constexpr unsigned long LONG_HOLD_MS = 1200UL; // increase hold threshold for noisy inputs

void checkButtons()
{
    // 1. Оновлення годинника (кожні 500мс)
    const unsigned long now = millis();
    if (now - lastClockUpdate >= 500)
    {
        lastClockUpdate = now;
        drawUniversalFooter();
    }

    // Якщо ще в ігнорі (щойно увійшли в екран/ініціалізуємо) — очищуємо джерела переривань і виходимо
    if (now < buttonsIgnoreUntil) {
        // прочитати та скидати прапори переривань, щоб вони не накопичувалися
        io.interruptSource();
        return;
    }

    // 2. ФІЛЬТР ПЕРЕРИВАННЯ: якщо немає сигналу від nINT і нічого не затиснуто — виходимо
    bool holdingActive = buttonWasPressed[BTN_UP] || buttonWasPressed[BTN_DOWN] || 
                         buttonWasPressed[BTN_OK] || buttonWasPressed[BTN_BACK];

    if (digitalRead(SX1509_INT_PIN) == HIGH && !holdingActive)
        return;

    // 3. Зчитуємо чисті фізичні рівні з SX1509
    bool up   = (io.digitalRead(BTN_UP) == LOW);
    bool down = (io.digitalRead(BTN_DOWN) == LOW);
    bool ok   = (io.digitalRead(BTN_OK) == LOW);
    bool back = (io.digitalRead(BTN_BACK) == LOW);

    if (!up && !down)
        upDownTestLatched = false;

    if (up && down)
    {
        if (!upDownTestLatched)
        {
            upDownTestLatched = true;
            runGuiTest();
            resetButtonsAndIgnore(300UL);
        }
        return;
    }

    if (upDownTestLatched)
        return;

    // ==========================================
    // ЛОГІКА ОБРОБКИ: КНОПКА UP (ПІН 3)
    // ==========================================
    if (up && !buttonWasPressed[BTN_UP]) {
        buttonPressStart[BTN_UP] = now;
        buttonWasPressed[BTN_UP] = true;
        holdTriggered[BTN_UP] = false;
    }
    if (!up && buttonWasPressed[BTN_UP]) {
        if (!holdTriggered[BTN_UP]) { // Спрацьовує чистий КЛІК при відпусканні
            if (currentState == MAIN_MENU) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                currentItem = (currentItem + 1) % totalItems;
                drawMainMenu();
            }
            else if (currentState == SUB_RADIO) {
                handleRadioButtons(true, false, false, false); // Збільшення гучності / цифри
            }
            else if (currentState == SUB_WIFI || currentState == SUB_BLE) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                if (hackerSubStage == 0) selectedTargetIndex = (selectedTargetIndex + 1) % 5;
                else if (hackerSubStage == 1) selectedAttackType = (selectedAttackType - 1 + 2) % 2;
            }
            else if (currentState == SUB_COMPASS) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                GUI::modeNext();
                drawVisualCompass();
            }
        }
        buttonWasPressed[BTN_UP] = false;
    }
    // ДОВГЕ УТРИМАННЯ UP: Зміна режиму ARK/RADIO (тільки в екрані радіо)
    if (up && buttonWasPressed[BTN_UP] && !holdTriggered[BTN_UP] && (now - buttonPressStart[BTN_UP]) >= LONG_HOLD_MS) {
        holdTriggered[BTN_UP] = true;
        if (currentState == SUB_RADIO) {
            radioModeArk = !radioModeArk;
            if (radioModeArk) currentRadioFreq = 561U;
            else { currentRadioFreq = 10150U; receiverModulationFm = true; }
            applyRadioSettings();
            ModuleSound::play(ModuleSound::SFX_OK);
            drawRadioScreen(); // Перемальовуємо екран після зміни режиму
        }
    }

    // ==========================================
    // ЛОГІКА ОБРОБКИ: КНОПКА DOWN (ПІН 4)
    // ==========================================
    if (down && !buttonWasPressed[BTN_DOWN]) {
        buttonPressStart[BTN_DOWN] = now;
        buttonWasPressed[BTN_DOWN] = true;
        holdTriggered[BTN_DOWN] = false;
    }
    if (!down && buttonWasPressed[BTN_DOWN]) {
        if (!holdTriggered[BTN_DOWN]) { // Спрацьовує чистий КЛІК при відпусканні
            if (currentState == MAIN_MENU) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                currentItem = (currentItem - 1 + totalItems) % totalItems;
                drawMainMenu();
            }
            else if (currentState == SUB_RADIO) {
                handleRadioButtons(false, true, false, false); // Зменшення гучності / цифри
            }
            else if (currentState == SUB_WIFI || currentState == SUB_BLE) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                if (hackerSubStage == 0) selectedTargetIndex = (selectedTargetIndex - 1 + 5) % 5;
                else if (hackerSubStage == 1) selectedAttackType = (selectedAttackType + 1) % 2;
            }
            else if (currentState == SUB_COMPASS) {
                ModuleSound::play(ModuleSound::SFX_CLICK);
                GUI::modePrev();
                drawVisualCompass();
            }
        }
        buttonWasPressed[BTN_DOWN] = false;
    }
    // ДОВГЕ УТРИМАННЯ DOWN: Зміна модуляції AM/FM (тільки в екрані радіо)
    if (down && buttonWasPressed[BTN_DOWN] && !holdTriggered[BTN_DOWN] && (now - buttonPressStart[BTN_DOWN]) >= LONG_HOLD_MS) {
        holdTriggered[BTN_DOWN] = true;
        if (currentState == SUB_RADIO && !radioModeArk) {
            receiverModulationFm = !receiverModulationFm;
            applyRadioSettings();
            ModuleSound::play(ModuleSound::SFX_OK);
            drawRadioScreen(); // Перемальовуємо екран
        }
    }

    // ==========================================
    // ЛОГІКА ОБРОБКИ: КНОПКА OK (ПІН 2)
    // ==========================================
    if (ok && !buttonWasPressed[BTN_OK]) {
        buttonPressStart[BTN_OK] = now;
        buttonWasPressed[BTN_OK] = true;
    }
    if (!ok && buttonWasPressed[BTN_OK]) {
        if (currentState == MAIN_MENU) {
            ModuleSound::play(ModuleSound::SFX_OK);
            gfx->fillScreen(BLACK);
            hackerSubStage = 0; selectedTargetIndex = 0; selectedAttackType = 0;

            if (currentItem == 0) { currentState = SUB_RADAR; drawVisualRadarScreen(); }
            else if (currentItem == 1) { currentState = SUB_COMPASS; drawVisualCompass(); }
            else if (currentItem == 2) { currentState = SUB_WIFI; ModuleCyber::startWiFiScan(); drawWiFiScreen(); }
            else if (currentItem == 3) { currentState = SUB_BLE; ModuleCyber::startBLEScan(); drawBLEScreen(); }
            else if (currentItem == 4) { 
                    currentState = SUB_RADIO; 
                    // Очистити стан кнопок при вході в Radio, щоб уникнути хаотичних натискань
                    for (int i = 0; i < 16; ++i) { buttonWasPressed[i] = false; holdTriggered[i] = false; buttonPressStart[i] = 0; }
                                // Ігнорувати обробку кнопок на короткий час, щоб пропустити апаратний шум/ініціалізацію
                                buttonsIgnoreUntil = millis() + 800UL; // longer ignore for noisy/touch-like buttons
                                ModuleRadio::init(); ModuleRadio::setAMBand(100U, 1150U, 561U, 10U); drawRadioScreen(); }
        }
        else if (currentState == SUB_RADIO) {
            handleRadioButtons(false, false, true, false); // Клік на OK в Радіо
        }
        else if (currentState == SUB_WIFI || currentState == SUB_BLE) {
            ModuleSound::play(ModuleSound::SFX_OK);
            if (hackerSubStage == 0) {
                if (currentState == SUB_WIFI) ModuleCyber::stopWiFiScan(); else ModuleCyber::stopBLEScan();
                hackerSubStage = 1;
            }
            else if (hackerSubStage == 1) {
                hackerSubStage = 2;
                if (currentState == SUB_WIFI) {
                    ModuleCyber::WiFiNetwork targetNet = ModuleCyber::getWiFiNetwork(selectedTargetIndex);
                    if (selectedAttackType == 0) ModuleCyber::startWiFiDeauth(targetNet.bssid, targetNet.channel);
                    else { strncpy(ModuleCyber::targetSsidClone, targetNet.ssid, 31); ModuleCyber::targetSsidClone[31] = '\0'; ModuleCyber::startWiFiBeaconSpam(); }
                }
                else if (currentState == SUB_BLE) { ModuleCyber::startBLEJammer(); }
            }
        }
        buttonWasPressed[BTN_OK] = false;
    }

    // ==========================================
    // ЛОГІКА ОБРОБКИ: КНОПКА BACK (ПІН 1)
    // ==========================================
    if (back && !buttonWasPressed[BTN_BACK]) {
        buttonPressStart[BTN_BACK] = now;
        buttonWasPressed[BTN_BACK] = true;
    }
    if (!back && buttonWasPressed[BTN_BACK]) {
        if (currentState == SUB_RADIO) {
            handleRadioButtons(false, false, false, true); // Клік на BACK в Радіо
        }
        else if (currentState == SUB_WIFI || currentState == SUB_BLE) {
            ModuleSound::play(ModuleSound::SFX_BACK);
            if (hackerSubStage == 2) {
                if (currentState == SUB_WIFI) { ModuleCyber::stopWiFiAttack(); ModuleCyber::targetSsidClone[0] = '\0'; }
                else { ModuleCyber::stopBLEAttack(); }
                hackerSubStage = 1; gfx->fillScreen(BLACK);
            }
            else if (hackerSubStage == 1) {
                hackerSubStage = 0; gfx->fillScreen(BLACK);
                if (currentState == SUB_WIFI) ModuleCyber::startWiFiScan(); else ModuleCyber::startBLEScan();
            }
            else {
                if (currentState == SUB_WIFI) ModuleCyber::stopWiFiScan(); else ModuleCyber::stopBLEScan();
                resetButtonsAndIgnore(800UL);
                currentState = MAIN_MENU; drawMainMenu();
            }
        }
        else if (currentState == SUB_COMPASS || currentState == SUB_RADAR) {
            ModuleSound::play(ModuleSound::SFX_BACK);
            resetButtonsAndIgnore(800UL);
            currentState = MAIN_MENU;
            ModuleGPS::stop();
            drawMainMenu();
        }
        buttonWasPressed[BTN_BACK] = false;
    }

    // 4. Скидання прапора подій на SX1509
    io.interruptSource();
}


}