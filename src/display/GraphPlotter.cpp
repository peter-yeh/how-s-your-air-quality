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
    : originX(x), originY(y), width(w), height(h), stepX(w / (MAX_HISTORY - 1)), minScale(0), maxScale(maxVal), historyCount(0)
{
}

int16_t GraphPlotter::mapY(float val) const
{
    float clamped = constrain(val, minScale, maxScale);
    return (originY + height) - (int16_t)(((clamped - minScale) / (maxScale - minScale)) * height);
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

    display.setCursor(originX, originY + height + 4);
    display.print("-30s");
    display.setCursor(originX + width - 18, originY + height + 4);
    display.print("now");

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
    else
    {
        // Shift old values left to discard the oldest sample
        for (uint8_t i = 0; i < MAX_HISTORY - 1; ++i)
        {
            historyPM1[i] = historyPM1[i + 1];
            historyPM25[i] = historyPM25[i + 1];
            historyPM10[i] = historyPM10[i + 1];
        }
        historyPM1[MAX_HISTORY - 1] = pm1;
        historyPM25[MAX_HISTORY - 1] = pm25;
        historyPM10[MAX_HISTORY - 1] = pm10;
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

    if (historyCount == 1)
    {
        display.drawPixel(originX, mapY(historyPM1[0]), COLOR_PM1);
        display.drawPixel(originX, mapY(historyPM25[0]), COLOR_PM25);
        display.drawPixel(originX, mapY(historyPM10[0]), COLOR_PM10);
        return;
    }

    // Draw lines connecting consecutive points
    for (uint8_t i = 1; i < historyCount; ++i)
    {
        int16_t x1 = originX + (i - 1) * stepX;
        int16_t x2 = originX + i * stepX;

        display.drawLine(x1, mapY(historyPM1[i - 1]), x2, mapY(historyPM1[i]), COLOR_PM1);
        display.drawLine(x1, mapY(historyPM25[i - 1]), x2, mapY(historyPM25[i]), COLOR_PM25);
        display.drawLine(x1, mapY(historyPM10[i - 1]), x2, mapY(historyPM10[i]), COLOR_PM10);
    }
}

void GraphPlotter::reset()
{
    historyCount = 0;
}
