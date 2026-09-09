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
    constexpr int16_t BASE_GRAPH_Y = 96;
    constexpr uint8_t NORMAL_FONT_SIZE = 2;
    constexpr uint8_t SMALL_FONT_SIZE = 1;
    constexpr int16_t UPTIME_X = 10;
    constexpr int16_t UPTIME_Y = 5;

    Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, -1);
    GraphPlotter graph;

    AirQualitySummary currentSummary;
    uint32_t currentUptimeSeconds = 0;
    bool hasSummary = false;
    bool needsPMRedraw = false;
    bool needsGraphRedraw = false;
    int16_t screenShiftX = 0;
    int16_t screenShiftY = 0;
    String currentClock = "--:--:--";
    bool currentWifiConnected = false;
    bool currentBluetoothConnected = false;

    String formatUptime(uint32_t uptimeSeconds)
    {
        const uint32_t hours = uptimeSeconds / 3600;
        const uint8_t minutes = (uptimeSeconds / 60) % 60;
        const uint8_t seconds = uptimeSeconds % 60;
        String result = "Up ";
        result += hours;
        result += ":";
        if (minutes < 10)
        {
            result += "0";
        }
        result += minutes;
        result += ":";
        if (seconds < 10)
        {
            result += "0";
        }
        result += seconds;
        return result;
    }

    void drawUptime()
    {
        display.fillRect(5 + screenShiftX, UPTIME_Y + screenShiftY, 90, 8, ST77XX_BLACK);
        display.setTextSize(SMALL_FONT_SIZE);
        display.setTextColor(ST77XX_WHITE);
        display.setCursor(UPTIME_X + screenShiftX, UPTIME_Y + screenShiftY);
        display.print(formatUptime(currentUptimeSeconds));
    }

    void drawSummary()
    {
        display.fillRect(34 + screenShiftX, 29 + screenShiftY, 205, 51, ST77XX_BLACK);
        display.setTextSize(SMALL_FONT_SIZE);
        display.setTextColor(ST77XX_WHITE);
        display.setCursor(66 + screenShiftX, 29 + screenShiftY);
        display.print("Low");
        display.setCursor(105 + screenShiftX, 29 + screenShiftY);
        display.print("High");
        display.setCursor(148 + screenShiftX, 29 + screenShiftY);
        display.print("Avg");

        const int16_t rowY[] = {40, 55, 70};
        const uint16_t rowColors[] = {ST77XX_CYAN, ST77XX_YELLOW, ST77XX_MAGENTA};
        const char *labels[] = {"PM1", "PM25", "PM10"};
        const float lowValues[] = {currentSummary.lowPm1, currentSummary.lowPm25, currentSummary.lowPm10};
        const float highValues[] = {currentSummary.highPm1, currentSummary.highPm25, currentSummary.highPm10};
        const float averageValues[] = {currentSummary.averagePm1, currentSummary.averagePm25, currentSummary.averagePm10};

        for (uint8_t i = 0; i < 3; ++i)
        {
            display.setTextColor(rowColors[i]);
            display.setCursor(36 + screenShiftX, rowY[i] + screenShiftY);
            display.print(labels[i]);
            display.setTextColor(ST77XX_WHITE);
            display.setCursor(66 + screenShiftX, rowY[i] + screenShiftY);
            display.print((int)(lowValues[i] + 0.5f));
            display.setCursor(105 + screenShiftX, rowY[i] + screenShiftY);
            display.print((int)(highValues[i] + 0.5f));
            display.setCursor(148 + screenShiftX, rowY[i] + screenShiftY);
            display.print((int)(averageValues[i] + 0.5f));
            display.setCursor(190 + screenShiftX, rowY[i] + screenShiftY);
            display.print("ug/m3");
        }
    }

    void drawBluetoothIcon(bool connected)
    {
        const int16_t iconX = 296 + screenShiftX;
        const int16_t iconY = 13 + screenShiftY;
        const uint16_t color = connected ? ST77XX_CYAN : ST77XX_RED;

        display.fillRect(iconX - 5, iconY - 6, 11, 9, ST77XX_BLACK);
        display.drawLine(iconX, iconY - 6, iconX, iconY + 2, color);
        display.drawLine(iconX, iconY - 6, iconX + 4, iconY - 3, color);
        display.drawLine(iconX + 4, iconY - 3, iconX - 4, iconY + 1, color);
        display.drawLine(iconX - 4, iconY - 5, iconX + 4, iconY - 2, color);
        display.drawLine(iconX + 4, iconY - 2, iconX, iconY + 2, color);
    }

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
    display.setCursor(BASE_GRAPH_X, 20);
    display.println("Air Quality");
    showStatus("--:--:--", false, false, 0);

    // Initialize graph frame, scale, and grid
    graph.setPosition(BASE_GRAPH_X, BASE_GRAPH_Y);
    graph.init(display);

    lastUpdateMs = millis();
    Serial.println("Display initialized.");
}

void DisplayController::update()
{
    if (needsPMRedraw)
    {
        needsPMRedraw = false;

        if (hasSummary)
        {
            drawSummary();
        }
    }

    if (needsGraphRedraw)
    {
        needsGraphRedraw = false;
        graph.draw(display);
    }
}

void DisplayController::showStats(const AirQualitySummary &summary)
{
    currentSummary = summary;
    hasSummary = true;
    needsPMRedraw = true;
}

void DisplayController::addGraphSample(float pm1, float pm25, float pm10)
{
    graph.addSample(pm1, pm25, pm10);
    needsGraphRedraw = true;
}

void DisplayController::showStatus(const char *timeText, bool wifiConnected, bool bluetoothConnected, uint32_t uptimeSeconds)
{
    currentClock = timeText;
    currentWifiConnected = wifiConnected;
    currentBluetoothConnected = bluetoothConnected;
    currentUptimeSeconds = uptimeSeconds;
    drawUptime();
    display.fillRect(234 + screenShiftX, 7 + screenShiftY, 81, 16, ST77XX_BLACK);
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(240 + screenShiftX, 7 + screenShiftY);
    display.print(timeText);
    drawBluetoothIcon(bluetoothConnected);
    drawWifiIcon(wifiConnected);
}

void DisplayController::shiftScreen(int16_t x, int16_t y)
{
    screenShiftX = x;
    screenShiftY = y;
    display.fillScreen(ST77XX_BLACK);

    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(NORMAL_FONT_SIZE);
    display.setCursor(BASE_GRAPH_X + screenShiftX, 20 + screenShiftY);
    display.println("Air Quality");

    graph.setPosition(BASE_GRAPH_X + screenShiftX, BASE_GRAPH_Y + screenShiftY);
    graph.init(display);
    showStatus(currentClock.c_str(), currentWifiConnected, currentBluetoothConnected, currentUptimeSeconds);
    if (hasSummary)
    {
        drawSummary();
    }
    needsPMRedraw = hasSummary;
    needsGraphRedraw = true;
}
