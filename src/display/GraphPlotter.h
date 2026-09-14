#pragma once

#include <stdint.h>
#include <Adafruit_GFX.h>

class GraphPlotter
{
public:
    enum class Mode
    {
        SECONDS = 0,
        MINUTES = 1,
        HOURS = 2
    };

    static constexpr uint8_t CAPACITY = 120; // Collect up to 120 points

    GraphPlotter(int16_t x = 36, int16_t y = 96, int16_t w = 261, int16_t h = 120, float maxVal = 100.0f);

    void init(Adafruit_GFX &display);
    bool addSample(float pm1, float pm25, float pm10);
    void draw(Adafruit_GFX &display);
    void setPosition(int16_t x, int16_t y);
    void reset();
    void setMode(Mode mode);
    Mode getMode() const { return currentMode; }

private:
    struct Point
    {
        float pm1;
        float pm25;
        float pm10;
    };

    int16_t mapY(float val, float minScale, float maxScale) const;
    void drawGrid(Adafruit_GFX &display);
    void updateScale(float &minScale, float &maxScale, const Point *history, uint8_t count) const;
    void drawScaleLabels(Adafruit_GFX &display, float minScale, float maxScale);
    void drawTimeLabels(Adafruit_GFX &display, uint32_t totalSeconds);
    void drawPlaceholder(Adafruit_GFX &display);

    int16_t originX;
    int16_t originY;
    int16_t width;
    int16_t height;
    float maxValScale;

    Mode currentMode = Mode::SECONDS;

    Point secondsHistory[CAPACITY];
    uint8_t secondsCount = 0;

    Point minutesHistory[CAPACITY];
    uint8_t minutesCount = 0;
    uint8_t secondsToMinuteCounter = 0;
    Point minuteAccumulator = {0, 0, 0};

    Point hoursHistory[CAPACITY];
    uint8_t hoursCount = 0;
    uint16_t secondsToHourCounter = 0;
    Point hourAccumulator = {0, 0, 0};
};
