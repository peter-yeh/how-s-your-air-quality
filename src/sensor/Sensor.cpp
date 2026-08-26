#include "Sensor.h"

SensorController::~SensorController()
{
    if (bmv)
    {
        delete bmv;
        bmv = nullptr;
    }
}

void SensorController::scanI2C()
{
    Serial.println("\n+======================================================+");
    Serial.println("|          BOARD I2C SCANNER (IO32=SDA, IO25=SCL)      |");
    Serial.println("+======================================================+");

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
            Serial.printf("[I2C SCAN] >>> SUCCESS! Found BMV080 at address 0x%02X <<<\n", addr);
            break;
        }
    }

    if (detectedAddr == 0)
    {
        Serial.println("[I2C SCAN] Probing all addresses 0x08 to 0x77 on SDA=32, SCL=25...");
        for (uint8_t addr = 0x08; addr <= 0x77; addr++)
        {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0)
            {
                detectedAddr = addr;
                Serial.printf("  >>> Found I2C device at 0x%02X <<<\n", addr);
                break;
            }
        }
    }

    if (detectedAddr != 0)
    {
        Serial.printf("[DIAGNOSIS] SUCCESS: Sensor detected on IO32/IO25 at address 0x%02X!\n", detectedAddr);
    }
    else
    {
        Serial.println("[DIAGNOSIS] No response on IO32/IO25. If using 4 cables without CSB tied to 3.3V,");
        Serial.println("            connect CSB to 3.3V, or check the JST 4-pin wire order.");
    }
    Serial.println("+======================================================+\n");
}

bool SensorController::begin()
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
        Serial.printf("ERROR: BMV080 failed I2C connection on SDA=32, SCL=25 at 0x%02X\n", detectedAddr);
        return false;
    }
    Serial.printf("BMV080: I2C connection OK (SDA=32, SCL=25, Address 0x%02X).\n", detectedAddr);

    uint16_t status = bmv->openBmv080();
    if (status != 0)
    {
        Serial.print("ERROR: BMV080 openBmv080 failed, status = ");
        Serial.println(status);
        return false;
    }
    Serial.println("BMV080: Sensor initialized.");

    // These features improve stability and make a blocked optical path visible.
    if (!bmv->setObstructionDetection(true))
    {
        Serial.println("WARNING: BMV080 obstruction detection could not be enabled.");
    }
    if (!bmv->setDoVibrationFiltering(true))
    {
        Serial.println("WARNING: BMV080 vibration filtering could not be enabled.");
    }

    // FAST_RESPONSE changes the estimation algorithm, not the measurement rate.
    // BALANCED avoids amplifying startup noise while still responding promptly.
    int algorithmResult = bmv->setMeasurementAlgorithm(BALANCED);
    if (algorithmResult != 0)
    {
        Serial.printf("WARNING: BMV080 measurement algorithm setup failed (%d).\n", algorithmResult);
    }

    int modeResult = bmv->setBmv080Mode(CONTINUOUS_MODE);
    if (modeResult != 0)
    {
        Serial.print("ERROR: BMV080 setBmv080Mode failed, result = ");
        Serial.println(modeResult);
        return false;
    }
    Serial.println("BMV080: Continuous mode started (BALANCED).");

    initialized = true;
    return true;
}

bool SensorController::read(float &pm1, float &pm25, float &pm10)
{
    if (!initialized || !bmv)
    {
        return false;
    }

    if (!bmv->getBmv080Data(&pm1, &pm25, &pm10))
    {
        return false;
    }

    static bool previousObstructed = false;
    bool obstructed = bmv->ifObstructed();
    if (obstructed != previousObstructed)
    {
        Serial.println(obstructed
                           ? "WARNING: BMV080 reports an obstructed optical path."
                           : "BMV080 optical path is clear.");
        previousObstructed = obstructed;
    }

    return true;
}
