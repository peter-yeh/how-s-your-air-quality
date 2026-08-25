#pragma once

#include <stdint.h>
#include <Adafruit_GFX.h>

class GraphPlotter
{
public:
    static constexpr uint8_t MAX_HISTORY = 30;

    GraphPlotter(int16_t x = 36, int16_t y = 96, int16_t w = 261, int16_t h = 120, float maxVal = 100.0f);

    void init(Adafruit_GFX &display);
    void addSample(float pm1, float pm25, float pm10);
    void draw(Adafruit_GFX &display);
    void reset();

private:
    int16_t mapY(float val) const;
    void drawGrid(Adafruit_GFX &display);

    int16_t originX;
    int16_t originY;
    int16_t width;
    int16_t height;
    int16_t stepX;
    float maxScale;

    float historyPM1[MAX_HISTORY];
    float historyPM25[MAX_HISTORY];
    float historyPM10[MAX_HISTORY];
    uint8_t historyCount = 0;
};
