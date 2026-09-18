#ifndef MOD_CYBER_H
#define MOD_CYBER_H

#include <Arduino.h>

namespace ModuleCyber
{

    // Структура для реальних Wi-Fi точок доступу з фіксованою довжиною
    struct WiFiNetwork
    {
        char ssid[33];    // Явний розмір масиву (ім'я мережі до 32 символів)
        uint8_t bssid[6]; // Явний розмір масиву під MAC-адресу
        int32_t rssi;
        uint8_t channel;
    };

    // Структура для реальних Bluetooth пристроїв з фіксованою довжиною
    struct BLEDeviceData
    {
        char name[30];   // Явний розмір масиву під ім'я пристрою
        char macStr[18]; // Явний розмір масиву під MAC-рядок
        int32_t rssi;
    };

    // Глобальні змінні атак, які має бачити main.cpp на Ядрі 0
    extern bool isDeauthActive;
    extern bool isBLEJamActive;
    extern bool isBeaconSpamActive; // !!! ДОДАНО ДЛЯ ЯДРА 0

    extern uint8_t deauthPacket[26]; // Явно вказано розмір пакету в 26 байт
  
   // ВИПРАВЛЕНО: Сюди gui.cpp просто покладе ім'я цілі, оригінальний масив не постраждає
    extern char targetSsidClone[32]; 
      // ВИПРАВЛЕНО: Додано const для повної синхронізації з файлом .cpp
    extern const char* fakeSsids[]; 

    // Ініціалізація бездротових інтерфейсів
    void init();

    // Керування режимами Wi-Fi
    void startWiFiScan();
    void stopWiFiScan();
    void startWiFiDeauth(uint8_t *targetBssid, uint8_t channel);
    void startWiFiBeaconSpam();
    void stopWiFiAttack();
    void sendSingleBeacon(); // Додайте в кінець списку функцій у заголовку

    // Керування режимами BLE
    void startBLEScan();
    void stopBLEScan();
    void startBLEJammer();
    void stopBLEAttack();

    // Геттери даних для відображення в gui.cpp
    int getWiFiCount();
    WiFiNetwork getWiFiNetwork(int index);
    int getBLECount();
    BLEDeviceData getBLEDevice(int index);
}

#endif
