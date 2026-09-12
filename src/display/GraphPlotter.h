#pragma once

#include <stdint.h>
#include <Adafruit_GFX.h>

class GraphPlotter
{
public:
    static constexpr uint8_t CAPACITY = 120; // Collect up to 120 points, then collapse to half

    GraphPlotter(int16_t x = 36, int16_t y = 96, int16_t w = 261, int16_t h = 120, float maxVal = 100.0f);

    void init(Adafruit_GFX &display);
    void addSample(float pm1, float pm25, float pm10);
    void draw(Adafruit_GFX &display);
    void setPosition(int16_t x, int16_t y);
    void reset();

private:
    struct Point
    {
        float pm1;
        float pm25;
        float pm10;
    };

    int16_t mapY(float val) const;
    void drawGrid(Adafruit_GFX &display);
    void updateScale();
    void drawScaleLabels(Adafruit_GFX &display);
    void drawTimeLabels(Adafruit_GFX &display);
    void collapse();
    uint16_t getTimeLabel(uint8_t index, bool isEnd = false) const;

    int16_t originX;
    int16_t originY;
    int16_t width;
    int16_t height;
    int16_t stepX;
    float minScale;
    float maxScale;
    uint16_t timeUnitMinutes; // Current time per data point

    Point history[CAPACITY];
    uint8_t historyCount = 0;
};
