#ifndef GUI_BLE_H
#define GUI_BLE_H

#include <Arduino.h>

namespace GUI {
    // === BLUETOOTH АТАКИ ===
    
    // Основна функція відрисовки BLE екрана
    void drawBLEScreen();
    
    // Обробник кнопок для BLE режиму (в core)
    // handleBLEButtons() вирішуються в core
}

#endif
