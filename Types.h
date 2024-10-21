#ifndef TYPES_H
#define TYPES_H

#include "kitepp.hpp"
#include <iostream>
#include <vector>
#include <chrono>
namespace kc = kiteconnect;

// Structure to store 15-minute candle data
struct Candle {
    double open;
    double high;
    double low;
    double close;
    std::string color; // Green or Red
    std::chrono::time_point<std::chrono::system_clock> startTime;
    std::chrono::time_point<std::chrono::system_clock> endTime;
    double bodyRatio{0};
    double wickRatio{0};
    double candleToIndexRatio{0};
};
// Structure to track day-high and day-low for each scrip (reused)
struct ScripData {
    std::vector<Candle> candles; // To store the last 2 candles
    double dayHigh;
    double dayLow;
    bool DayLowReversalIdentified = false;
    bool DayHighReversalIdentified = false;
    double signalCandleHigh = 0.0; // Price to monitor for placing a buy order
    double signalCandleLow = 0.0;
    bool orderPlaced = false; // Track if an order is placed
};


#endif // TYPES_H