#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <cmath>

struct AirQualitySummary
{
    // Initialize with NaN to distinguish "no data" from "clean air (0.0)"
    float lowPm1 = std::numeric_limits<float>::quiet_NaN();
    float highPm1 = std::numeric_limits<float>::quiet_NaN();
    float averagePm1 = std::numeric_limits<float>::quiet_NaN();
    float medianPm1 = std::numeric_limits<float>::quiet_NaN();
    float lowPm25 = std::numeric_limits<float>::quiet_NaN();
    float highPm25 = std::numeric_limits<float>::quiet_NaN();
    float averagePm25 = std::numeric_limits<float>::quiet_NaN();
    float medianPm25 = std::numeric_limits<float>::quiet_NaN();
    float lowPm10 = std::numeric_limits<float>::quiet_NaN();
    float highPm10 = std::numeric_limits<float>::quiet_NaN();
    float averagePm10 = std::numeric_limits<float>::quiet_NaN();
    float medianPm10 = std::numeric_limits<float>::quiet_NaN();

    String toString() const;
};

class AirQualityStats
{
public:
    void addSample(float pm1, float pm25, float pm10);
    bool getSummary(AirQualitySummary &summary) const;
    void clear();
    size_t getCount() const;

private:
    // MedianEstimator: Implements P-squared algorithm for online median estimation.
    // THREADING: NOT THREAD-SAFE. Designed for single-threaded use only.
    // Must be called only from the thread that reads sensor data.
    // Do not call add()/get()/clear() concurrently from multiple threads.
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