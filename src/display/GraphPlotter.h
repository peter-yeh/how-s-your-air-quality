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

    GraphPlotter(int16_t x = 36, int16_t y = 96, int16_t w = 261, int16_t h = 120, float = 100.0f);

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

    struct History
    {
        Point samples[CAPACITY];
        Point total = {0, 0, 0};
        uint8_t count = 0;
        uint16_t samplesSinceAverage = 0;
    };

    int16_t mapY(float val, float minScale, float maxScale) const;
    static uint8_t modeIndex(Mode mode);
    static void addPoint(History &history, const Point &point);
    static bool addAverageSample(History &history, const Point &sample, uint16_t interval);
    void drawGrid(Adafruit_GFX &display);
    void updateScale(float &minScale, float &maxScale, const Point *history, uint8_t count) const;
    void drawScaleLabels(Adafruit_GFX &display, float minScale, float maxScale);
    void drawTimeLabels(Adafruit_GFX &display, uint32_t totalSeconds);
    void drawPlaceholder(Adafruit_GFX &display);

    int16_t originX;
    int16_t originY;
    int16_t width;
    int16_t height;

    static constexpr uint8_t HISTORY_COUNT = 3;
    Mode currentMode = Mode::SECONDS;
    History histories[HISTORY_COUNT];
};
