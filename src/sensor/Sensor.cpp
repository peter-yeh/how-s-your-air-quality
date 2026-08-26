#include "Sensor.h"

bool SensorController::begin()
{
    Wire.begin();

    // Check I2C connection
    if (bmv.begin() != 0)
    {
        Serial.println("ERROR: BMV080 sensor not detected over I2C!");
        Serial.println("Check wiring (3V3, GND, SDA, SCL) and solder joints.");
        return false;
    }
    Serial.println("BMV080: I2C connection OK.");

    // Initialize the BMV080 sensor (creates internal handle)
    uint16_t status = bmv.openBmv080();
    if (status != 0)
    {
        Serial.print("ERROR: BMV080 openBmv080 failed, status = ");
        Serial.println(status);
        return false;
    }
    Serial.println("BMV080: Sensor initialized.");

    // Start continuous measurement mode
    int modeResult = bmv.setBmv080Mode(CONTINUOUS_MODE);
    if (modeResult != 0)
    {
        Serial.print("ERROR: BMV080 setBmv080Mode failed, result = ");
        Serial.println(modeResult);
        return false;
    }
    Serial.println("BMV080: Continuous mode started.");

    initialized = true;
    return true;
}

bool SensorController::read(float &pm1, float &pm25, float &pm10)
{
    if (!initialized)
    {
        return false;
    }

    // getBmv080Data returns true when new data is ready
    return bmv.getBmv080Data(&pm1, &pm25, &pm10);
}
