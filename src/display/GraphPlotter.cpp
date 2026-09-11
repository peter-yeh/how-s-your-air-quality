#include "GraphPlotter.h"
#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <math.h>

namespace
{
    constexpr uint16_t COLOR_GRID = 0x2104;   // Subtle dark gray
    constexpr uint16_t COLOR_BORDER = 0x5AEB; // Medium gray
    constexpr uint16_t COLOR_PM1 = ST77XX_CYAN;
    constexpr uint16_t COLOR_PM25 = ST77XX_YELLOW;
    constexpr uint16_t COLOR_PM10 = ST77XX_MAGENTA;
}

GraphPlotter::GraphPlotter(int16_t x, int16_t y, int16_t w, int16_t h, float maxVal)
    : originX(x), originY(y), width(w), height(h), stepX(0), minScale(0), maxScale(maxVal),
      timeUnitMinutes(1), historyCount(0)
{
}

int16_t GraphPlotter::mapY(float val) const
{
    float clamped = constrain(val, minScale, maxScale);
    int32_t yOffset = (int32_t)(((clamped - minScale) / (maxScale - minScale)) * height);
    return (int32_t)originY + height - yOffset;
}

void GraphPlotter::drawGrid(Adafruit_GFX &display)
{
    // Horizontal grid lines at 25%, 50%, 75%
    for (int i = 1; i <= 3; ++i)
    {
        int16_t y = originY + (height * i) / 4;
        for (int16_t x = originX + 1; x < originX + width; x += 4)
        {
            display.drawPixel(x, y, COLOR_GRID);
        }
    }
}

void GraphPlotter::init(Adafruit_GFX &display)
{
    // Graph Frame & Scale Labels
    display.drawRect(originX, originY, width + 1, height + 1, COLOR_BORDER);
    drawScaleLabels(display);
    drawGrid(display);
}

void GraphPlotter::updateScale()
{
    float largest = historyPM1[0];

    for (uint8_t i = 0; i < historyCount; ++i)
    {
        largest = max(largest, max(historyPM1[i], max(historyPM25[i], historyPM10[i])));
    }

    minScale = 0.0f;
    maxScale = ceilf(max(largest * 1.1f, largest + 1.0f));

    if (maxScale <= minScale)
    {
        maxScale = minScale + 2.0f;
    }
}

void GraphPlotter::drawScaleLabels(Adafruit_GFX &display)
{
    display.fillRect(0, originY - 6, originX - 1, height + 12, ST77XX_BLACK);
    display.setTextSize(1);
    display.setTextColor(COLOR_BORDER);
    display.setCursor(originX - 18, originY - 3);
    display.print((int)maxScale);
    display.setCursor(originX - 18, originY + (height / 2) - 3);
    display.print((int)((minScale + maxScale) / 2.0f));
    display.setCursor(originX - 18, originY + height - 4);
    display.print((int)minScale);
}

uint16_t GraphPlotter::getTimeLabel(uint8_t index, bool isEnd) const
{
    // Calculate time in minutes from the reference point
    // index 0 is oldest, historyCount-1 is newest
    uint16_t minutesAgo;

    if (isEnd)
    {
        minutesAgo = 0; // "now"
    }
    else
    {
        minutesAgo = (historyCount - 1) * timeUnitMinutes;
    }

    return minutesAgo;
}

void GraphPlotter::drawTimeLabels(Adafruit_GFX &display)
{
    display.setTextSize(1);
    display.setTextColor(COLOR_BORDER);

    // Clear the bottom time label area
    display.fillRect(originX - 10, originY + height + 4, width + 30, 10, ST77XX_BLACK);

    // Left label (newest - now)
    display.setCursor(originX - 2, originY + height + 4);
    display.print("now");

    // Middle label
    uint16_t midTime = (getTimeLabel(0, false) / 2);
    display.setCursor(originX + width / 2 - 8, originY + height + 4);
    if (midTime >= 60)
    {
        display.print("-");
        display.print(midTime / 60);
        display.print("h");
    }
    else if (midTime > 0)
    {
        display.print("-");
        display.print(midTime);
        display.print("m");
    }
    else
    {
        display.print("0m");
    }

    // Right label (oldest)
    uint16_t rightTime = getTimeLabel(0, false);
    display.setCursor(originX + width - 14, originY + height + 4);
    if (rightTime >= 60)
    {
        display.print("-");
        display.print(rightTime / 60);
        display.print("h");
    }
    else
    {
        display.print("-");
        display.print(rightTime);
        display.print("m");
    }
}

void GraphPlotter::collapse()
{
    // Average consecutive pairs to reduce from MAX_HISTORY to MIN_HISTORY
    for (uint8_t i = 0; i < MIN_HISTORY; ++i)
    {
        uint8_t srcIdx = i * 2; // Source pairs: 0-1, 2-3, 4-5, etc.
        historyPM1[i] = (historyPM1[srcIdx] + historyPM1[srcIdx + 1]) / 2.0f;
        historyPM25[i] = (historyPM25[srcIdx] + historyPM25[srcIdx + 1]) / 2.0f;
        historyPM10[i] = (historyPM10[srcIdx] + historyPM10[srcIdx + 1]) / 2.0f;
    }

    historyCount = MIN_HISTORY;
    timeUnitMinutes *= 2; // Each point now represents 2 minutes instead of 1
}

void GraphPlotter::setPosition(int16_t x, int16_t y)
{
    originX = x;
    originY = y;
}

void GraphPlotter::addSample(float pm1, float pm25, float pm10)
{
    if (historyCount < MAX_HISTORY)
    {
        historyPM1[historyCount] = pm1;
        historyPM25[historyCount] = pm25;
        historyPM10[historyCount] = pm10;
        historyCount++;
    }
    else if (historyCount == MAX_HISTORY)
    {
        // Add one more sample to reach MAX_HISTORY, then collapse
        historyPM1[historyCount] = pm1;
        historyPM25[historyCount] = pm25;
        historyPM10[historyCount] = pm10;
        historyCount++;
        collapse();
    }
    else
    {
        // We've collapsed, now shift and add like normal (but with larger time steps)
        for (uint8_t i = 0; i < MIN_HISTORY - 1; ++i)
        {
            historyPM1[i] = historyPM1[i + 1];
            historyPM25[i] = historyPM25[i + 1];
            historyPM10[i] = historyPM10[i + 1];
        }
        historyPM1[MIN_HISTORY - 1] = pm1;
        historyPM25[MIN_HISTORY - 1] = pm25;
        historyPM10[MIN_HISTORY - 1] = pm10;
    }
}

void GraphPlotter::draw(Adafruit_GFX &display)
{
    // Clear graph interior
    display.fillRect(originX + 1, originY + 1, width - 1, height - 1, ST77XX_BLACK);

    // Draw grid
    drawGrid(display);

    if (historyCount == 0)
    {
        return;
    }

    updateScale();
    drawScaleLabels(display);
    drawTimeLabels(display);

    if (historyCount == 1)
    {
        display.drawPixel(originX, mapY(historyPM1[0]), COLOR_PM1);
        display.drawPixel(originX, mapY(historyPM25[0]), COLOR_PM25);
        display.drawPixel(originX, mapY(historyPM10[0]), COLOR_PM10);
        return;
    }

    // Draw lines connecting consecutive points
    // Progressive zoom: use actual historyCount instead of MAX_HISTORY for spacing
    for (uint8_t i = 1; i < historyCount; ++i)
    {
        int16_t x1 = originX + width - (int32_t)(i - 1) * width / (historyCount - 1);
        int16_t x2 = originX + width - (int32_t)i * width / (historyCount - 1);

        display.drawLine(x1, mapY(historyPM1[i - 1]), x2, mapY(historyPM1[i]), COLOR_PM1);
        display.drawLine(x1, mapY(historyPM25[i - 1]), x2, mapY(historyPM25[i]), COLOR_PM25);
        display.drawLine(x1, mapY(historyPM10[i - 1]), x2, mapY(historyPM10[i]), COLOR_PM10);
    }
}

void GraphPlotter::reset()
{
    historyCount = 0;
    timeUnitMinutes = 1;
}
