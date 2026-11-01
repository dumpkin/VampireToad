#ifndef GUI_RADIO_H
#define GUI_RADIO_H

#include <Arduino.h>
#include "mod_buttons.h"

namespace GUI {
    // === РАДІО-ЕКРАН ===
    
    // Основна функція відрисовки радіо-екрана
    void drawRadioScreen();

    void initializeRadio();
    
    // Обробник кнопок для радіо-режиму
    void handleRadioButtons(const ModuleButtons::Event &event);
}

#endif
