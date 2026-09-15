#pragma once

#include <stddef.h>
#include <stdint.h>

struct AirQualitySummary
{
    float lowPm1 = 0;
    float highPm1 = 0;
    float averagePm1 = 0;
    float medianPm1 = 0;
    float lowPm25 = 0;
    float highPm25 = 0;
    float averagePm25 = 0;
    float medianPm25 = 0;
    float lowPm10 = 0;
    float highPm10 = 0;
    float averagePm10 = 0;
    float medianPm10 = 0;
};

class AirQualityStats
{
public:
    void addSample(float pm1, float pm25, float pm10);
    bool getSummary(AirQualitySummary &summary) const;
    void clear();
    size_t getCount() const;

private:
    class MedianEstimator
    {
    public:
        void add(float value);
        float get() const;
        void clear();

    private:
        static constexpr uint8_t MARKER_COUNT = 5;

        float initialValues[MARKER_COUNT] = {};
        float markerHeights[MARKER_COUNT] = {};
        float markerPositions[MARKER_COUNT] = {};
        float desiredPositions[MARKER_COUNT] = {};
        float positionIncrements[MARKER_COUNT] = {};
        uint8_t initialCount = 0;
    };

    size_t count = 0;
    double sumPm1 = 0;
    double sumPm25 = 0;
    double sumPm10 = 0;
    float lowPm1 = 0;
    float highPm1 = 0;
    float lowPm25 = 0;
    float highPm25 = 0;
    float lowPm10 = 0;
    float highPm10 = 0;
    MedianEstimator medianPm1;
    MedianEstimator medianPm25;
    MedianEstimator medianPm10;
};