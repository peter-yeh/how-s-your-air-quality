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
    : originX(x), originY(y), width(w), height(h), maxValScale(maxVal), currentMode(Mode::MINUTES)
{
}

int16_t GraphPlotter::mapY(float val, float minScale, float maxScale) const
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
    drawGrid(display);
}

void GraphPlotter::updateScale(float &minScale, float &maxScale, const Point *history, uint8_t count) const
{
    float largest = count > 0 ? history[0].pm1 : 0;

    for (uint8_t i = 0; i < count; ++i)
    {
        largest = max(largest, max(history[i].pm1, max(history[i].pm25, history[i].pm10)));
    }

    minScale = 0.0f;
    maxScale = ceilf(max(largest * 1.1f, largest + 1.0f));

    if (maxScale <= minScale)
    {
        maxScale = minScale + 2.0f;
    }
}

void GraphPlotter::drawScaleLabels(Adafruit_GFX &display, float minScale, float maxScale)
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

void GraphPlotter::drawTimeLabels(Adafruit_GFX &display, uint32_t span)
{
    display.setTextSize(1);
    display.setTextColor(COLOR_BORDER);

    // Clear the bottom time label area
    display.fillRect(originX - 10, originY + height + 4, width + 30, 10, ST77XX_BLACK);

    const char *unit = "s";
    if (currentMode == Mode::MINUTES)
        unit = "m";
    else if (currentMode == Mode::HOURS)
        unit = "h";

    // Left label (oldest - full time span)
    display.setCursor(originX - 2, originY + height + 4);
    display.print("-");
    display.print(span);
    display.print(unit);

    // Middle label (half the time span)
    uint32_t midSpan = span / 2;
    display.setCursor(originX + width / 2 - 8, originY + height + 4);
    if (midSpan > 0)
    {
        display.print("-");
        display.print(midSpan);
        display.print(unit);
    }
    else
    {
        display.print("0");
        display.print(unit);
    }

    // Right label (newest - now)
    display.setCursor(originX + width - 14, originY + height + 4);
    display.print("now");
}

void GraphPlotter::setPosition(int16_t x, int16_t y)
{
    originX = x;
    originY = y;
}

void GraphPlotter::setMode(Mode mode)
{
    currentMode = mode;
}

bool GraphPlotter::addSample(float pm1, float pm25, float pm10)
{
    bool updatedCurrentMode = false;

    // 1. Seconds history
    if (secondsCount == CAPACITY)
    {
        memmove(secondsHistory, secondsHistory + 1, (CAPACITY - 1) * sizeof(Point));
        secondsHistory[CAPACITY - 1] = {pm1, pm25, pm10};
    }
    else
    {
        secondsHistory[secondsCount++] = {pm1, pm25, pm10};
    }
    if (currentMode == Mode::SECONDS)
        updatedCurrentMode = true;

    // 2. Minutes history
    minuteAccumulator.pm1 += pm1;
    minuteAccumulator.pm25 += pm25;
    minuteAccumulator.pm10 += pm10;
    secondsToMinuteCounter++;

    if (secondsToMinuteCounter >= 60 || minutesCount == 0)
    {
        Point avg = {minuteAccumulator.pm1 / secondsToMinuteCounter, minuteAccumulator.pm25 / secondsToMinuteCounter, minuteAccumulator.pm10 / secondsToMinuteCounter};
        if (minutesCount == CAPACITY)
        {
            memmove(minutesHistory, minutesHistory + 1, (CAPACITY - 1) * sizeof(Point));
            minutesHistory[CAPACITY - 1] = avg;
        }
        else
        {
            minutesHistory[minutesCount++] = avg;
        }
        minuteAccumulator = {0, 0, 0};
        secondsToMinuteCounter = 0;
        if (currentMode == Mode::MINUTES)
            updatedCurrentMode = true;
    }

    // 3. Hours history
    hourAccumulator.pm1 += pm1;
    hourAccumulator.pm25 += pm25;
    hourAccumulator.pm10 += pm10;
    secondsToHourCounter++;

    if (secondsToHourCounter >= 3600 || hoursCount == 0)
    {
        Point avg = {hourAccumulator.pm1 / secondsToHourCounter, hourAccumulator.pm25 / secondsToHourCounter, hourAccumulator.pm10 / secondsToHourCounter};
        if (hoursCount == CAPACITY)
        {
            memmove(hoursHistory, hoursHistory + 1, (CAPACITY - 1) * sizeof(Point));
            hoursHistory[CAPACITY - 1] = avg;
        }
        else
        {
            hoursHistory[hoursCount++] = avg;
        }
        hourAccumulator = {0, 0, 0};
        secondsToHourCounter = 0;
        if (currentMode == Mode::HOURS)
            updatedCurrentMode = true;
    }

    return updatedCurrentMode;
}

void GraphPlotter::draw(Adafruit_GFX &display)
{
    // Clear graph interior
    display.fillRect(originX + 1, originY + 1, width - 1, height - 1, ST77XX_BLACK);

    // Draw grid
    drawGrid(display);

    const Point *history;
    uint8_t count;
    uint32_t span;

    if (currentMode == Mode::SECONDS)
    {
        history = secondsHistory;
        count = secondsCount;
        span = count > 0 ? (count - 1) : 0;
    }
    else if (currentMode == Mode::MINUTES)
    {
        history = minutesHistory;
        count = minutesCount;
        span = count > 0 ? (count - 1) : 0;
    }
    else
    {
        history = hoursHistory;
        count = hoursCount;
        span = count > 0 ? (count - 1) : 0;
    }

    if (count == 0)
    {
        drawPlaceholder(display);
        return;
    }

    float minScale, maxScale;
    updateScale(minScale, maxScale, history, count);
    drawScaleLabels(display, minScale, maxScale);
    drawTimeLabels(display, span);

    if (count == 1)
    {
        display.drawPixel(originX, mapY(history[0].pm1, minScale, maxScale), COLOR_PM1);
        display.drawPixel(originX, mapY(history[0].pm25, minScale, maxScale), COLOR_PM25);
        display.drawPixel(originX, mapY(history[0].pm10, minScale, maxScale), COLOR_PM10);
        return;
    }

    // Draw lines connecting consecutive points
    // index 0 (oldest) is drawn at the left, newest at the right
    for (uint8_t i = 1; i < count; ++i)
    {
        int16_t x1 = originX + (int32_t)(i - 1) * width / (count - 1);
        int16_t x2 = originX + (int32_t)i * width / (count - 1);

        display.drawLine(x1, mapY(history[i - 1].pm1, minScale, maxScale), x2, mapY(history[i].pm1, minScale, maxScale), COLOR_PM1);
        display.drawLine(x1, mapY(history[i - 1].pm25, minScale, maxScale), x2, mapY(history[i].pm25, minScale, maxScale), COLOR_PM25);
        display.drawLine(x1, mapY(history[i - 1].pm10, minScale, maxScale), x2, mapY(history[i].pm10, minScale, maxScale), COLOR_PM10);
    }
}

void GraphPlotter::reset()
{
    secondsCount = 0;
    minutesCount = 0;
    secondsToMinuteCounter = 0;
    minuteAccumulator = {0, 0, 0};
    hoursCount = 0;
    secondsToHourCounter = 0;
    hourAccumulator = {0, 0, 0};
}

void GraphPlotter::drawPlaceholder(Adafruit_GFX &display)
{
    static const char *const MESSAGE = "No data yet";
    constexpr int16_t CHAR_WIDTH = 6; // 6px per character at text size 1
    constexpr int16_t textWidth = 11 * CHAR_WIDTH;

    display.setTextSize(1);
    display.setTextColor(COLOR_BORDER);
    display.setCursor(originX + (width - textWidth) / 2, originY + height / 2 - 4);
    display.print(MESSAGE);
}
