#ifndef GUI_COMPASS_H
#define GUI_COMPASS_H

#include <Arduino.h>

namespace GUI {
    // === КОМПАС ТА IMU-ЕКРАН ===
    
    // Основна функція відрисовки компаса
  void drawVisualCompass();
    void modeNext();
    void modePrev();
    int getSelectedMode();
}

#endif
