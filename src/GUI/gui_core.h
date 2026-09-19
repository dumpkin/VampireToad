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
 // Тепер це номери фізичних пінів на самому чіпі SX1509
#define BTN_BACK  1
#define BTN_OK    2
#define BTN_UP    4
#define BTN_DOWN  3


// Пін переривання nINT, який фізично припаяний до ESP32-S3
#define SX1509_INT_PIN 4


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

    // Запустити діагностичний тест кнопок і дисплея
    void runGuiTest();

    // Скинути стан кнопок і ігнорувати обробку на вказаний ms (щоб уникнути шуму при переходах)
    void resetButtonsAndIgnore(unsigned long ms);
}

#endif
