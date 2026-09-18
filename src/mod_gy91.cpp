#include "mod_gy91.h"
#include <Wire.h>
#include <math.h>
#include <Adafruit_BMP280.h>

namespace ModuleGY91
{

    NavigationData currentData;
    bool isInitialized = false;

    unsigned long lastUpdateTime = 0;
    float gyroBiasZ = 0.0;
    float baseAltitude = 0.0;

    Adafruit_BMP280 bmp;

    bool writeRegister(uint8_t i2cAddr, uint8_t reg, uint8_t value)
    {
        Wire.beginTransmission(i2cAddr);
        Wire.write(reg);
        Wire.write(value);
        return (Wire.endTransmission() == 0);
    }

    void readRegisters(uint8_t i2cAddr, uint8_t reg, uint8_t *buffer, uint8_t count)
    {
        Wire.beginTransmission(i2cAddr);
        Wire.write(reg);
        Wire.endTransmission(false);

        uint8_t bytesReceived = Wire.requestFrom(i2cAddr, count);
        for (uint8_t i = 0; i < bytesReceived && i < count; i++)
        {
            buffer[i] = Wire.read();
        }
    }

    bool isSensorReady() { return isInitialized; }
    NavigationData getData() { return currentData; }

    void resetDeadReckoning()
    {
        currentData.deadReckoningX = 0.0f;
        currentData.deadReckoningY = 0.0f;
        currentData.internalSpeed = 0.0f;
        if (isInitialized)
        {
            baseAltitude = bmp.readAltitude(1013.25);
        }
    }

    void init()
    {
        Wire.begin(GY91_SDA_PIN, GY91_SCL_PIN, 400000U);
        delay(100);

        if (!bmp.begin(BMP280_ADDR))
        {
            isInitialized = false;
            return;
        }

        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);

        // Налаштування MPU9250
        writeRegister(MPU9250_ADDR, 0x6B, 0x80); // Скидання живлення
        delay(50);
        writeRegister(MPU9250_ADDR, 0x6B, 0x03); // Тактування від гіроскопа Z
        writeRegister(MPU9250_ADDR, 0x1C, 0x08); // Акселерометр +-4g
        writeRegister(MPU9250_ADDR, 0x1B, 0x08); // Гіроскоп +-500 dps

        // !!! КРИТИЧНО: Вмикаємо I2C Bypass Mode, щоб бачити AK8963 напряму на загальній шині
        writeRegister(MPU9250_ADDR, 0x37, 0x02);
        delay(20);

        // Налаштування магнітометра AK8963
        writeRegister(AK8963_ADDR, 0x0A, 0x00); // Power Down режим перед зміною налаштувань
        delay(10);
        writeRegister(AK8963_ADDR, 0x0A, 0x16); // Continuous measurement mode 2 (100Hz) + 16-bit вивід
        delay(20);

        // Калібрування нуля гіроскопа Z
        float gyroSum = 0;
        uint8_t gyroBuf[2] = {0, 0};
        for (int i = 0; i < 50; i++)
        {
            readRegisters(MPU9250_ADDR, 0x47, gyroBuf, 2);
            int16_t gz = (gyroBuf[0] << 8) | gyroBuf[1];
            gyroSum += gz;
            delay(2);
        }
        gyroBiasZ = (gyroSum / 50.0f) / 65.5f;

        isInitialized = true;
        baseAltitude = bmp.readAltitude(1013.25);

        // Початкове зчитування магнітометра для стартової точки курсу
        uint8_t magBuf[7];
        readRegisters(AK8963_ADDR, 0x03, magBuf, 7);
        int16_t mx = (magBuf[1] << 8) | magBuf[0];
        int16_t my = (magBuf[3] << 8) | magBuf[2];
        float startHeading = atan2((float)my, (float)mx) * 180.0f / M_PI;
        if (startHeading < 0)
            startHeading += 360.0f;
        currentData.heading = startHeading;

        resetDeadReckoning();
        lastUpdateTime = micros();
    }

    void update()
    {
        if (!isInitialized)
            return;

        unsigned long currentTime = micros();
        float dt = (currentTime - lastUpdateTime) / 1000000.0f;
        lastUpdateTime = currentTime;
        if (dt <= 0.0f || dt > 0.5f)
            return;

        // 1. Опитування акселерометра та гіроскопа MPU9250
        uint8_t imuBuf[14] = {0};
        readRegisters(MPU9250_ADDR, 0x3B, imuBuf, 14);

        int16_t rawX = (imuBuf[0] << 8) | imuBuf[1];
        int16_t rawY = (imuBuf[2] << 8) | imuBuf[3];
        int16_t rawZ = (imuBuf[4] << 8) | imuBuf[5];
        int16_t rawGZ = (imuBuf[12] << 8) | imuBuf[13];

        float ax = (rawX / 8192.0f) * 9.80665f;
        float ay = (rawY / 8192.0f) * 9.80665f;
        float az = (rawZ / 8192.0f) * 9.80665f;
        float gz = ((rawGZ / 65.5f) - gyroBiasZ); // Перевід у градуси/сек

        // 2. Опитування магнітометра AK8963 (Регістри 0x03 - 0x09)
        uint8_t magBuf[7] = {0};
        readRegisters(AK8963_ADDR, 0x03, magBuf, 7);

        // КРИТИЧНИЙ ФІКС: Магнітометр потребує обов'язкового читання ST2 (magBuf[6]),
        // інакше він блокує оновлення даних. Також перевіряємо біт готовності HOFL (0x08)
        bool magDataReady = (magBuf[6] & 0x01);
        bool magOverflow = (magBuf[6] & 0x08);

        if (magDataReady && !magOverflow && (magBuf[1] != 0 || magBuf[3] != 0))
        {
            // Читаємо сирі дані (враховуємо інверсію осей GY-91 відносно MPU)
            int16_t mx = (magBuf[1] << 8) | magBuf[0];
            int16_t my = (magBuf[3] << 8) | magBuf[2];

            if (mx != 0 || my != 0)
            {
                // Рахуємо кут (в оригіналі GY-91 осі X та Y компаса можуть бути переплутані)
                float magHeading = atan2((float)my, (float)mx) * 180.0f / M_PI;
                if (magHeading < 0)
                    magHeading += 360.0f;

                // Комплементарний фільтр: 95% довіри гіроскопу, 5% компасу для стабілізації
                float gyroHeading = currentData.heading + gz * dt;

                if (magHeading - gyroHeading > 180.0f)
                    gyroHeading += 360.0f;
                else if (gyroHeading - magHeading > 180.0f)
                    gyroHeading -= 360.0f;

                currentData.heading = 0.95f * gyroHeading + 0.05f * magHeading;
            }
        }
        else
        {
            // Якщо магнітометр зайнятий або переповнений — крутимо суто по гіроскопу, щоб шкала не "замерзала"
            currentData.heading += gz * dt;

            // Якщо зафіксовано переповнення (HOFL), перезапускаємо режим вимірювання магнітометра
            if (magOverflow)
            {
                writeRegister(AK8963_ADDR, 0x0A, 0x00);
                delay(2);
                writeRegister(AK8963_ADDR, 0x0A, 0x16);
            }
        }

        // Нормалізація кута в межі 0-360
        if (currentData.heading >= 360.0f)
            currentData.heading -= 360.0f;
        if (currentData.heading < 0.0f)
            currentData.heading += 360.0f;

        // 3. ФІКС ЗРОСТАННЯ ШВИДКОСТІ (Dead Reckoning)
        // Підвищуємо поріг шуму (deadband) з 0.25f до 0.55f, щоб відсікти дрейф нерухомого датчика
        float linearAX = ax;
        float linearAY = ay;
        if (fabs(linearAX) < 0.55f)
            linearAX = 0.0f;
        if (fabs(linearAY) < 0.55f)
            linearAY = 0.0f;

        float accelMagnitude = sqrt(linearAX * linearAX + linearAY * linearAY);
        if (accelMagnitude > 0.0f)
        {
            currentData.internalSpeed += accelMagnitude * dt;
        }
        else
        {
            // Жорсткіше гальмування швидкості в спокої (множник 0.85 замість 0.95)
            currentData.internalSpeed *= 0.85f;
            if (currentData.internalSpeed < 0.05f)
                currentData.internalSpeed = 0.0f;
        }

        float headingRad = currentData.heading * M_PI / 180.0f;
        currentData.deadReckoningX += currentData.internalSpeed * sin(headingRad) * dt;
        currentData.deadReckoningY += currentData.internalSpeed * cos(headingRad) * dt;

        currentData.accelX = linearAX;
        currentData.accelY = linearAY;
        currentData.accelZ = az;

        // 4. Оновлення барометра
        currentData.pressure = bmp.readPressure() / 100.0f;
        currentData.altitude = bmp.readAltitude(1013.25) - baseAltitude;
        currentData.temperature = bmp.readTemperature();
    }
    float getTemperature() { return currentData.temperature; }
}
