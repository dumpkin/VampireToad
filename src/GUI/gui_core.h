#ifndef GUI_CORE_H
#define GUI_CORE_H

#include <Arduino.h>

namespace GUI {
    enum MenuState { 
        MAIN_MENU, 
        SUB_RADAR, 
        SUB_COMPASS, 
        SUB_WIFI, 
        SUB_BLE,
        SUB_RADIO 
    };

    // Кнопки
    #define BTN_UP     4
    #define BTN_DOWN   5
    #define BTN_OK     6
    #define BTN_BACK   7

    // === ГЛОБАЛЬНІ ЗМІННІ ===
    extern MenuState currentState;
    extern int currentItem;
    extern int hackerSubStage;
    extern int selectedTargetIndex;
    extern int selectedAttackType;

    // === ЯДРОВА ЛОГІКА ===
    
    // Ініціалізація GUI системи
    void init();
    
    // Основний обробник кнопок (відправляє команди в відповідні модулі)
    void checkButtons();
    
    // Отримати поточний стан меню
    MenuState getCurrentState();
    
    // Основна функція відрисовки головного меню
    void drawMainMenu();
}

#endif
