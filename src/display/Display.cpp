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
    constexpr int16_t BASE_GRAPH_X = 36;
    constexpr int16_t BASE_GRAPH_Y = 84;
    constexpr uint8_t NORMAL_FONT_SIZE = 2;
    constexpr uint8_t SMALL_FONT_SIZE = 1;

    Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, -1);
    GraphPlotter graph;

    float currentPM1 = 0;
    float currentPM25 = 0;
    float currentPM10 = 0;
    bool needsRedraw = false;
    int16_t screenShiftX = 0;
    int16_t screenShiftY = 0;
    String currentClock = "--:--:--";
    bool currentWifiConnected = false;

    void drawWifiIcon(bool connected)
    {
        const int16_t iconX = 310 + screenShiftX;
        const int16_t iconY = 13 + screenShiftY;
        const uint16_t color = connected ? ST77XX_GREEN : ST77XX_RED;

        // Clear compact bounding box around (310, 10) - 11px wide, 9px high
        display.fillRect(iconX - 5, iconY - 6, 11, 9, ST77XX_BLACK);

        // 1. Base Dot
        display.drawPixel(iconX, iconY, color);

        // 2. Inner Arc (Radius = 3)
        display.drawPixel(iconX - 2, iconY - 2, color);
        display.drawPixel(iconX - 1, iconY - 3, color);
        display.drawPixel(iconX, iconY - 3, color);
        display.drawPixel(iconX + 1, iconY - 3, color);
        display.drawPixel(iconX + 2, iconY - 2, color);

        // 3. Outer Arc (Radius = 5)
        display.drawPixel(iconX - 4, iconY - 4, color);
        display.drawPixel(iconX - 3, iconY - 5, color);
        display.drawPixel(iconX - 2, iconY - 6, color);
        display.drawPixel(iconX - 1, iconY - 6, color);
        display.drawPixel(iconX, iconY - 6, color);
        display.drawPixel(iconX + 1, iconY - 6, color);
        display.drawPixel(iconX + 2, iconY - 6, color);
        display.drawPixel(iconX + 3, iconY - 5, color);
    }
}

void DisplayController::begin()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    display.init(240, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    // Compact header and readings leave a 20-pixel edge margin.
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(NORMAL_FONT_SIZE);
    display.setCursor(20, 20);
    display.println("Air Quality Monitor");
    showStatus("--:--:--", false);

    // Static Value Labels (Color-coded to match graph curves)
    display.setTextSize(NORMAL_FONT_SIZE);

    display.setTextColor(ST77XX_CYAN);
    display.setCursor(20, 36);
    display.print("PM1.0:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145, 36);
    display.print("ug/m3");

    display.setTextColor(ST77XX_YELLOW);
    display.setCursor(20, 50);
    display.print("PM2.5:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145, 50);
    display.print("ug/m3");

    display.setTextColor(ST77XX_MAGENTA);
    display.setCursor(20, 64);
    display.print("PM10 :");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145, 64);
    display.print("ug/m3");

    // Initialize graph frame, scale, and grid
    graph.setPosition(BASE_GRAPH_X, BASE_GRAPH_Y);
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
    display.fillRect(100 + screenShiftX, 36 + screenShiftY, 45, 16, ST77XX_BLACK);
    display.fillRect(100 + screenShiftX, 50 + screenShiftY, 45, 16, ST77XX_BLACK);
    display.fillRect(100 + screenShiftX, 64 + screenShiftY, 45, 16, ST77XX_BLACK);

    display.setTextSize(NORMAL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);

    display.setCursor(100 + screenShiftX, 36 + screenShiftY);
    display.print((int)(currentPM1 + 0.5f));

    display.setCursor(100 + screenShiftX, 50 + screenShiftY);
    display.print((int)(currentPM25 + 0.5f));

    display.setCursor(100 + screenShiftX, 64 + screenShiftY);
    display.print((int)(currentPM10 + 0.5f));

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

void DisplayController::showStatus(const char *timeText, bool wifiConnected)
{
    currentClock = timeText;
    currentWifiConnected = wifiConnected;
    display.fillRect(244 + screenShiftX, 14 + screenShiftY, 56, 12, ST77XX_BLACK);
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(250 + screenShiftX, 17 + screenShiftY);
    display.print(timeText);
    drawWifiIcon(wifiConnected);
}

void DisplayController::shiftScreen(int16_t x, int16_t y)
{
    screenShiftX = x;
    screenShiftY = y;
    display.fillScreen(ST77XX_BLACK);

    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(NORMAL_FONT_SIZE);
    display.setCursor(20 + screenShiftX, 20 + screenShiftY);
    display.println("Air Quality Monitor");

    display.setTextSize(NORMAL_FONT_SIZE);
    display.setTextColor(ST77XX_CYAN);
    display.setCursor(20 + screenShiftX, 36 + screenShiftY);
    display.print("PM1.0:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145 + screenShiftX, 36 + screenShiftY);
    display.print("ug/m3");

    display.setTextColor(ST77XX_YELLOW);
    display.setCursor(20 + screenShiftX, 50 + screenShiftY);
    display.print("PM2.5:");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145 + screenShiftX, 50 + screenShiftY);
    display.print("ug/m3");

    display.setTextColor(ST77XX_MAGENTA);
    display.setCursor(20 + screenShiftX, 64 + screenShiftY);
    display.print("PM10 :");
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(145 + screenShiftX, 64 + screenShiftY);
    display.print("ug/m3");

    graph.setPosition(BASE_GRAPH_X + screenShiftX, BASE_GRAPH_Y + screenShiftY);
    graph.init(display);
    showStatus(currentClock.c_str(), currentWifiConnected);
    needsRedraw = true;
}
