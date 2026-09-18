#ifndef GUI_COMMON_H
#define GUI_COMMON_H

#include <Arduino.h>

namespace GUI {
    // === СПІЛЬНІ УТИЛІТИ ДЛЯ ВСІХ ЕКРАНІВ ===
    
    // Універсальна плашка статус-бару (годинник + вольтметр + температура)
    void drawUniversalFooter();
    
    // Підменю розмітка (рамка + заголовок)
    void drawSubMenuLayout(const char *title);
    
    // Розрахунок київського часового зміщення
    int getKyivTimeOffset(int year, int month, int day, int hour);
    
    // Отримати напругу батареї з кешуванням
    float getBatteryVoltage();
    
    // Векторна графіка атак (Wi-Fi / Bluetooth іконки)
    void drawHackerIcons(bool isWiFi, bool animate);
    
    // Сплеш-скрін при завантаженні
    void drawSplash();
}

#endif
