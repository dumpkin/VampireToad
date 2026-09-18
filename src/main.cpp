#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include "gui.h"
#include "mod_gps.h"
#include "mod_sound.h"
#include "mod_gy91.h"
#include "mod_cyber.h"

#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 15
#define TFT_DC 16
#define TFT_RST 14

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *physical_tft = new Arduino_ST7735(bus, TFT_RST, 1, false, 128, 160, 0, 0);
Arduino_GFX *gfx = new Arduino_Canvas(128, 160, physical_tft);

TinyGPSPlus gps;
static unsigned long lastFrameTick = 0;

TaskHandle_t Core0TaskHandle = NULL;

// === ФОНОВА ЗАДАЧА ДЛЯ CORE 0 (Радіо, Сенсори, Навігація) ===
void TaskCore0(void *pvParameters)
{
    (void)pvParameters;

    ModuleCyber::init();

    for (;;)
    {
        // 1. Фонове вичитування GPS
        while (Serial2.available() > 0)
        {
            gps.encode(Serial2.read());
        }

        // 2. Оновлення комплементарного фільтра інерціалки
        ModuleGY91::update();

        // 3. Скидання Dead Reckoning при валідному GPS
        if (gps.location.isValid() && gps.location.isUpdated())
        {
            ModuleGY91::resetDeadReckoning();
        }

        // БОЙОВИЙ ЦИКЛ 1: Wi-Fi Deauth Атака кожні 100мс
        if (ModuleCyber::isDeauthActive)
        {
            esp_wifi_80211_tx(WIFI_IF_STA, ModuleCyber::deauthPacket, 26, false);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // БОЙОВИЙ ЦИКЛ 2: BLE Глушилка
        if (ModuleCyber::isBLEJamActive)
        {
            ::BLEAdvertising *pAdvertising = ::BLEDevice::getAdvertising();
            pAdvertising->start();
            delayMicroseconds(500);
            pAdvertising->stop();
        }

        // Додайте цей БОЙОВИЙ ЦИКЛ 3 всередині функції TaskCore0 у вашому main.cpp
        if (ModuleCyber::isBeaconSpamActive)
        {
            // Безперервно спамимо фейковими точками з мікро-паузою 5мс між іменами
            ModuleCyber::sendSingleBeacon();
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    physical_tft->begin();
    physical_tft->setRotation(1);
    physical_tft->fillScreen(BLACK);

    gfx->begin();
    gfx->setRotation(1);
    gfx->fillScreen(BLACK);

    ModuleSound::init();
    ModuleGY91::init();
    GUI::init();
    ModuleGPS::init();

    // Створення потоку на Core 0
    xTaskCreatePinnedToCore(
        TaskCore0,
        "TaskCore0",
        4096,
        NULL,
        1,
        &Core0TaskHandle,
        0);

    GUI::drawSplash();
    gfx->flush();  // КРИТИЧНО: Виштовхуємо сплеш на дисплей перед drawMainMenu
    GUI::drawMainMenu();
}

void loop() {
    // Опитування кнопок з антибрязком
    GUI::checkButtons();

    // Рендеринг інтерфейсу залежно від активного стану операційної системи Penta OS
    switch (GUI::getCurrentState()) {
        case GUI::MAIN_MENU:
            break;
            
        case GUI::SUB_RADAR:
            GUI::drawVisualRadarScreen(); 
            break;
            
        case GUI::SUB_COMPASS: 
            GUI::drawVisualCompass(); 
            break;

        case GUI::SUB_WIFI:
            GUI::drawWiFiScreen(); 
            break;

        case GUI::SUB_BLE:
            GUI::drawBLEScreen();  
            break;

        // НОВЕ: Додано підтримку та виклик вікна радіоприймача
        case GUI::SUB_RADIO:
            GUI::drawRadioScreen(); 
            break;
            
        default:
            break;
    }

    // Безперервне виштовхування готового Canvas буфера на фізичну матрицю дисплея (30 FPS)
    if (millis() - lastFrameTick >= 33) {
        lastFrameTick = millis();
        gfx->flush(); 
    }
}
