// Sensor: Bosch BMV080 particulate matter sensor
// Interface: I2C through the DFRobot BMV080 driver

#include "Sensor.h"
#include <cmath>
#define LOG_CLASS "BMVSensor"
#include "../utilities/Logger.h"

BMVSensor::~BMVSensor()
{
    if (bmv)
    {
        delete bmv;
        bmv = nullptr;
    }
}

void BMVSensor::scanI2C()
{
    APP_LOG("+======================================================+");
    APP_LOG("|          BOARD I2C SCANNER (IO32=SDA, IO25=SCL)      |");
    APP_LOG("+======================================================+");

    // Exact pins from your Sunton 3.2" ESP32 LCD board silkscreen
    constexpr uint8_t PIN_SDA = 32;
    constexpr uint8_t PIN_SCL = 25;

    Wire.end();
    delay(20);
    Wire.begin(PIN_SDA, PIN_SCL, 100000);
    delay(20);

    const uint8_t candidateAddresses[] = {0x57, 0x56, 0x55, 0x54};
    detectedAddr = 0;

    for (uint8_t addr : candidateAddresses)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            detectedAddr = addr;
            APP_LOG("I2C scan success: found BMV080 at address 0x%02X", addr);
            break;
        }
    }

    if (detectedAddr == 0)
    {
        APP_LOG("Probing all addresses 0x08 to 0x77 on SDA=32, SCL=25...");
        for (uint8_t addr = 0x08; addr <= 0x77; addr++)
        {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0)
            {
                detectedAddr = addr;
                APP_LOG("Found I2C device at 0x%02X", addr);
                break;
            }
        }
    }

    if (detectedAddr != 0)
    {
        APP_LOG("Sensor detected on IO32/IO25 at address 0x%02X", detectedAddr);
    }
    else
    {
        APP_LOG("No response on IO32/IO25. If using 4 cables without CSB tied to 3.3V, connect CSB to 3.3V, or check the JST 4-pin wire order.");
    }
    APP_LOG("+======================================================+");
}

bool BMVSensor::begin()
{
    constexpr uint8_t PIN_SDA = 32;
    constexpr uint8_t PIN_SCL = 25;

    scanI2C();

    if (detectedAddr == 0)
    {
        detectedAddr = 0x57; // Default DFRobot address
    }

    if (bmv)
    {
        delete bmv;
        bmv = nullptr;
    }

    Wire.end();
    delay(10);
    Wire.begin(PIN_SDA, PIN_SCL, 100000);

    bmv = new DFRobot_BMV080_I2C(&Wire, detectedAddr);

    if (bmv->begin() != 0)
    {
        APP_LOG("BMV080 failed I2C connection on SDA=32, SCL=25 at 0x%02X", detectedAddr);
        return false;
    }
    APP_LOG("I2C connection OK, address 0x%02X", detectedAddr);

    uint16_t status = bmv->openBmv080();
    if (status != 0)
    {
        APP_LOG("BMV080 openBmv080 failed, status = %u", status);
        return false;
    }
    APP_LOG("BMV080 initialized");

    // Obstruction reporting is disabled; readings are still used by the graph.
    if (!bmv->setObstructionDetection(false))
    {
        APP_LOG("BMV080 obstruction detection could not be disabled.");
    }
    if (!bmv->setDoVibrationFiltering(true))
    {
        APP_LOG("BMV080 vibration filtering could not be enabled.");
    }

    // FAST_RESPONSE changes the estimation algorithm, not the measurement rate.
    // BALANCED avoids amplifying startup noise while still responding promptly.
    int algorithmResult = bmv->setMeasurementAlgorithm(BALANCED);
    if (algorithmResult != 0)
    {
        APP_LOG("BMV080 measurement algorithm setup failed (%d).", algorithmResult);
    }

    int modeResult = bmv->setBmv080Mode(CONTINUOUS_MODE);
    if (modeResult != 0)
    {
        APP_LOG("BMV080 setBmv080Mode failed, result = %d", modeResult);
        return false;
    }
    APP_LOG("BMV080 continuous mode started (BALANCED).");

    initialized = true;
    return true;
}

LatestReading BMVSensor::read()
{
    float pm1;
    float pm25;
    float pm10;

    if (bmv != nullptr &&
        bmv->getBmv080Data(&pm1, &pm25, &pm10) &&
        std::isfinite(pm1) &&
        std::isfinite(pm25) &&
        std::isfinite(pm10))
    {
        currentPm1 = pm1;
        currentPm25 = pm25;
        currentPm10 = pm10;
    }

    return LatestReading{currentPm1, currentPm25, currentPm10};
}
