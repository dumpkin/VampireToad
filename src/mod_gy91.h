#ifndef MOD_GY91_H
#define MOD_GY91_H

#include <Arduino.h>

namespace ModuleGY91
{
// Апаратні піни шини I2C
static constexpr uint8_t GY91_SDA_PIN = 8;
static constexpr uint8_t GY91_SCL_PIN = 9;

// I2C адреси чіпів у модулі GY-91
static constexpr uint8_t MPU9250_ADDR = 0x68;
static constexpr uint8_t BMP280_ADDR = 0x76;
static constexpr uint8_t AK8963_ADDR = 0x0C; // !!! Adresа вбудованого магнітометра

    // Структура для збереження відфільтрованих даних навігації
    struct NavigationData
    {
        float accelX, accelY, accelZ; // Очищене прискорення (м/с^2)
        float heading;                // Істинний магнітний курс (0-360) відносно Півночі
        float pressure;               // Атмосферний тиск (гПа)
        float altitude;               // Точна відносна висота (метри)
        float deadReckoningX;         // Нарахована X координата при втраті GPS (метри)
        float deadReckoningY;         // Нарахована Y координата при втраті GPS (метри)
        float internalSpeed;          // Поточна розрахована швидкість (м/с)
        float temperature;
    };

    void init();
    void update();
    bool isSensorReady();
    NavigationData getData();
    void resetDeadReckoning(); // Скидання координат у нуль при отриманні свіжого GPS FIX
}

#endif