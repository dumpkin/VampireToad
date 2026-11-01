#include "mod_gy91.h"
#include <Wire.h>
#include <math.h>
#include <Adafruit_BMP280.h>

namespace ModuleGY91
{

    NavigationData currentData;
    bool isInitialized = false;

    unsigned long lastUpdateTime = 0;
    float baseAltitude = 0.0;
    float gravityX = 0.0f;
    float gravityY = 0.0f;
    float gravityZ = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    bool gravityInitialized = false;
    bool headingInitialized = false;
    bool magneticHeadingInitialized = false;
    volatile bool deadReckoningResetPending = false;
    unsigned long stationarySince = 0;
    float gyroBiasZ = 0.0f;

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
        deadReckoningResetPending = true;
    }

    static void applyDeadReckoningReset()
    {
        currentData.deadReckoningX = 0.0f;
        currentData.deadReckoningY = 0.0f;
        currentData.internalSpeed = 0.0f;
        velocityX = 0.0f;
        velocityY = 0.0f;
        stationarySince = 0;
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

        float gyroSum = 0.0f;
        uint8_t gyroBuf[2] = {0, 0};
        constexpr uint8_t GYRO_CALIBRATION_SAMPLES = 100;
        for (uint8_t i = 0; i < GYRO_CALIBRATION_SAMPLES; ++i) {
            readRegisters(MPU9250_ADDR, 0x47, gyroBuf, sizeof(gyroBuf));
            const int16_t rawGZ = static_cast<int16_t>(
                (static_cast<uint16_t>(gyroBuf[0]) << 8) | gyroBuf[1]);
            gyroSum += rawGZ / 65.5f;
            delay(5);
        }
        gyroBiasZ = gyroSum / GYRO_CALIBRATION_SAMPLES;

        isInitialized = true;
        baseAltitude = bmp.readAltitude(1013.25);
        gravityInitialized = false;
        headingInitialized = true;
        magneticHeadingInitialized = false;
        currentData.heading = 0.0f;

        resetDeadReckoning();
        lastUpdateTime = micros();
    }

    void update()
    {
        if (!isInitialized)
            return;

        if (deadReckoningResetPending) {
            applyDeadReckoningReset();
            deadReckoningResetPending = false;
        }

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
        int16_t rawGZ = static_cast<int16_t>(
            (static_cast<uint16_t>(imuBuf[12]) << 8) | imuBuf[13]);
        float ax = (rawX / 8192.0f) * 9.80665f;
        float ay = (rawY / 8192.0f) * 9.80665f;
        float az = (rawZ / 8192.0f) * 9.80665f;
        float gz = rawGZ / 65.5f - gyroBiasZ;
        constexpr float GYRO_DEADBAND_DPS = 0.35f;
        if (fabsf(gz) < GYRO_DEADBAND_DPS) gz = 0.0f;

        // AK8963 ST1 is the data-ready flag; ST2 is only overflow/status.
        uint8_t magStatus = 0;
        readRegisters(AK8963_ADDR, 0x02, &magStatus, 1);
        bool magOverflow = false;
        bool magHeadingValid = false;
        float magHeading = 0.0f;

        if ((magStatus & 0x01) != 0)
        {
            uint8_t magBuf[7] = {0};
            readRegisters(AK8963_ADDR, 0x03, magBuf, 7);
            magOverflow = (magBuf[6] & 0x08) != 0;
            int16_t mx = (magBuf[1] << 8) | magBuf[0];
            int16_t my = (magBuf[3] << 8) | magBuf[2];

            if (!magOverflow && (mx != 0 || my != 0))
            {
                magHeading = atan2((float)my, (float)mx) * 180.0f / M_PI;
                if (magHeading < 0.0f) magHeading += 360.0f;
                magHeadingValid = true;
            }
        }

        if (magOverflow) {
            writeRegister(AK8963_ADDR, 0x0A, 0x00);
            delay(2);
            writeRegister(AK8963_ADDR, 0x0A, 0x16);
        }

        if (currentData.heading >= 360.0f) currentData.heading -= 360.0f;
        if (currentData.heading < 0.0f) currentData.heading += 360.0f;

        // Track gravity so a stationary, tilted board does not look like acceleration.
        if (!gravityInitialized) {
            gravityX = ax;
            gravityY = ay;
            gravityZ = az;
            gravityInitialized = true;
        }
        const float gravityAlpha = dt / (0.8f + dt);
        gravityX += (ax - gravityX) * gravityAlpha;
        gravityY += (ay - gravityY) * gravityAlpha;
        gravityZ += (az - gravityZ) * gravityAlpha;

        float linearAX = ax - gravityX;
        float linearAY = ay - gravityY;
        float linearAZ = az - gravityZ;
        constexpr float ACCELERATION_DEADBAND = 0.18f;
        if (fabsf(linearAX) < ACCELERATION_DEADBAND) linearAX = 0.0f;
        if (fabsf(linearAY) < ACCELERATION_DEADBAND) linearAY = 0.0f;
        if (fabsf(linearAZ) < ACCELERATION_DEADBAND) linearAZ = 0.0f;

        const float linearMagnitude = sqrtf(
            linearAX * linearAX + linearAY * linearAY + linearAZ * linearAZ);
        const float rawGyroRate = rawGZ / 65.5f;
        const bool stationary = linearMagnitude < 0.22f && fabsf(rawGyroRate) < 1.0f;
        if (stationary) {
            if (stationarySince == 0) stationarySince = millis();
            if (millis() - stationarySince >= 300UL) {
                const float biasAlpha = fminf(dt * 0.2f, 0.01f);
                gyroBiasZ += (rawGyroRate - gyroBiasZ) * biasAlpha;
                gz = 0.0f;
                velocityX = 0.0f;
                velocityY = 0.0f;
            }
        } else {
            stationarySince = 0;
        }

        if (headingInitialized) {
            currentData.heading += gz * dt;
        }

        if (magHeadingValid) {
            if (!magneticHeadingInitialized) {
                currentData.heading = magHeading;
                magneticHeadingInitialized = true;
            } else {
                float difference = magHeading - currentData.heading;
                if (difference > 180.0f) difference -= 360.0f;
                else if (difference < -180.0f) difference += 360.0f;

                if (fabsf(difference) <= 60.0f) {
                    if (fabsf(difference) < 0.5f) difference = 0.0f;
                    currentData.heading += difference * 0.05f;
                }
            }
        }

        if (currentData.heading >= 360.0f) currentData.heading -= 360.0f;
        if (currentData.heading < 0.0f) currentData.heading += 360.0f;

        if (!stationary && linearMagnitude > 0.0f) {
            velocityX += linearAX * dt;
            velocityY += linearAY * dt;
            const float damping = expf(-0.8f * dt);
            velocityX *= damping;
            velocityY *= damping;
        } else {
            const float damping = expf(-4.0f * dt);
            velocityX *= damping;
            velocityY *= damping;
        }

        currentData.internalSpeed = sqrtf(velocityX * velocityX + velocityY * velocityY);
        if (currentData.internalSpeed < 0.05f) {
            velocityX = 0.0f;
            velocityY = 0.0f;
            currentData.internalSpeed = 0.0f;
        }

        float headingRad = currentData.heading * M_PI / 180.0f;
        currentData.deadReckoningX += currentData.internalSpeed * sin(headingRad) * dt;
        currentData.deadReckoningY += currentData.internalSpeed * cos(headingRad) * dt;

        currentData.accelX = linearAX;
        currentData.accelY = linearAY;
        currentData.accelZ = linearAZ;

        // 4. Оновлення барометра
        currentData.pressure = bmp.readPressure() / 100.0f;
        currentData.altitude = bmp.readAltitude(1013.25) - baseAltitude;
        currentData.temperature = bmp.readTemperature();
    }
    float getTemperature() { return currentData.temperature; }
}
