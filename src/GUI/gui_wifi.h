#ifndef GUI_WIFI_H
#define GUI_WIFI_H

#include <Arduino.h>

namespace GUI {
    // === WI-FI АТАКИ ===
    
    // Основна функція відрисовки Wi-Fi екрана
    void drawWiFiScreen();
    
    // Обробник кнопок для Wi-Fi режиму (в core)
    // handleWiFiButtons() вирішуються в core
}

#endif
