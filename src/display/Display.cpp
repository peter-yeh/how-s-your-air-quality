#include "Display.h"

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

namespace
{
    constexpr uint8_t TFT_CS = 15;
    constexpr uint8_t TFT_DC = 2;
    constexpr uint8_t TFT_SCK = 14;
    constexpr uint8_t TFT_MOSI = 13;
    constexpr uint8_t TFT_MISO = 12;
    constexpr uint8_t TFT_BL = 27;
    constexpr uint32_t UPDATE_INTERVAL_MS = 1000;

    Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, -1);

    float currentPM1 = 0;
    float currentPM25 = 0;
    float currentPM10 = 0;
    bool dataReady = false;
}

void DisplayController::begin()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    display.init(240, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.setCursor(20, 30);
    display.println("Air Quality Monitor");

    // Draw static labels (never redrawn)
    display.setCursor(20, 70);
    display.print("PM1.0:");
    display.setCursor(220, 70);
    display.print("ug/m3");

    display.setCursor(20, 110);
    display.print("PM2.5:");
    display.setCursor(220, 110);
    display.print("ug/m3");

    display.setCursor(20, 150);
    display.print("PM10:");
    display.setCursor(220, 150);
    display.print("ug/m3");

    lastUpdateMs = millis();
    Serial.println("Display initialized.");
}

void DisplayController::update()
{
    uint32_t now = millis();
    if (now - lastUpdateMs < UPDATE_INTERVAL_MS)
    {
        return;
    }
    lastUpdateMs = now;

    if (!dataReady)
    {
        return;
    }

    // Clear and redraw all values
    display.fillRect(100, 70, 115, 20, ST77XX_BLACK);
    display.fillRect(100, 110, 115, 20, ST77XX_BLACK);
    display.fillRect(100, 150, 115, 20, ST77XX_BLACK);

    display.setTextSize(2);
    display.setCursor(100, 70);
    display.print(currentPM1, 1);

    display.setCursor(100, 110);
    display.print(currentPM25, 1);

    display.setCursor(100, 150);
    display.print(currentPM10, 1);
}

void DisplayController::showPM(float pm1Concentration, float pm25Concentration, float pm10Concentration)
{
    currentPM1 = pm1Concentration;
    currentPM25 = pm25Concentration;
    currentPM10 = pm10Concentration;
    dataReady = true;
}
