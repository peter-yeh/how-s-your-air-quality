#include "Display.h"
#include "GraphPlotter.h"

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
    GraphPlotter graph;

    float currentPM1 = 0;
    float currentPM25 = 0;
    float currentPM10 = 0;
    bool needsRedraw = false;
    bool sensorObstructed = false;
    bool sensorStatusAvailable = false;
}

void DisplayController::begin()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    display.init(240, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    // Title Header
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 6);
    display.println("Air Quality Monitor");

    // Static Value Labels (Color-coded to match graph curves)
    display.setTextSize(2);

    display.setTextColor(ST77XX_CYAN);
    display.setCursor(10, 28);
    display.print("PM1.0:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(185, 28);
    display.print("ug/m3");

    display.setTextColor(ST77XX_YELLOW);
    display.setCursor(10, 48);
    display.print("PM2.5:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(185, 48);
    display.print("ug/m3");

    display.setTextColor(ST77XX_MAGENTA);
    display.setCursor(10, 68);
    display.print("PM10 :");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(185, 68);
    display.print("ug/m3");

    // Initialize graph frame, scale, and grid
    graph.init(display);

    lastUpdateMs = millis();
    Serial.println("Display initialized.");
}

void DisplayController::update()
{
    if (!needsRedraw)
    {
        return;
    }
    needsRedraw = false;

    // Clear previous numbers and redraw top values
    display.fillRect(90, 28, 90, 16, ST77XX_BLACK);
    display.fillRect(90, 48, 90, 16, ST77XX_BLACK);
    display.fillRect(90, 68, 90, 16, ST77XX_BLACK);

    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);

    display.setCursor(90, 28);
    display.print(currentPM1, 1);

    display.setCursor(90, 48);
    display.print(currentPM25, 1);

    display.setCursor(90, 68);
    display.print(currentPM10, 1);

    display.fillRect(252, 0, 68, 20, ST77XX_BLACK);
    if (sensorStatusAvailable)
    {
        display.setTextSize(1);
        display.setTextColor(sensorObstructed ? ST77XX_RED : ST77XX_GREEN);
        display.setCursor(255, 6);
        display.print(sensorObstructed ? "LOADING" : "LIVE");
    }

    // Redraw graph with latest history
    graph.draw(display);
}

void DisplayController::showPM(float pm1Concentration, float pm25Concentration, float pm10Concentration)
{
    currentPM1 = pm1Concentration;
    currentPM25 = pm25Concentration;
    currentPM10 = pm10Concentration;

    // Add exactly one sample per valid measurement
    graph.addSample(currentPM1, currentPM25, currentPM10);
    needsRedraw = true;
}

void DisplayController::showSensorStatus(bool obstructed)
{
    sensorObstructed = obstructed;
    sensorStatusAvailable = true;
    needsRedraw = true;
}
