#ifndef GUI_CORE_H
#define GUI_CORE_H

#include <Arduino.h>

namespace GUI {
    enum MenuState { 
        MAIN_MENU, 
        SUB_NAVIGATION,
        SUB_WIFI, 
        SUB_BLE,
        SUB_RADIO 
    };

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

    // Скинути стан кнопок і ігнорувати обробку на вказаний ms (щоб уникнути шуму при переходах)
    void resetButtonsAndIgnore(unsigned long ms);
}

#endif
