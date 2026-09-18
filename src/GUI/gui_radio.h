#ifndef GUI_RADIO_H
#define GUI_RADIO_H

#include <Arduino.h>

namespace GUI {
    // === РАДІО-ЕКРАН ===
    
    // Основна функція відрисовки радіо-екрана
    void drawRadioScreen();
    
    // Обробник кнопок для радіо-режиму
    void handleRadioButtons(bool up, bool down, bool ok, bool back);
}

#endif
