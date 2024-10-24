#include "Logger.h"
#include "Types.h"
#include "orderManager.cpp"

// PatternDetector identifies technical patterns based on 15-minute candles
class PatternDetector {
  private:
    std::mutex patternMutex;

    PatternDetector() {} // Singleton pattern

  public:
    static PatternDetector& getInstance() {
        static PatternDetector instance;
        return instance;
    }

    void detectPattern(const double& instrumentToken, ScripData& scripData) {
        std::lock_guard<std::mutex> lock(patternMutex);
        if (scripData.candles.size() > 1) {
            Candle& prevCandle = scripData.candles[scripData.candles.size() - 2];
            Candle& currentCandle = scripData.candles.back();

            // if (currentCandle.bodyRatio >= 80) {

            Logger::getInstance().log(Logger::DEBUG, " ### Inside Detect Pattern for :", instrumentToken);
            auto candleToDayLowRatio =
                (std::abs(currentCandle.low - scripData.dayLow) / scripData.dayLow) * 100;
            auto candleToDayHighRatio =
                (std::abs(currentCandle.high - scripData.dayHigh) / scripData.dayHigh) * 100;

            if (prevCandle.color == "Red" && currentCandle.color == "Green" &&
                ((currentCandle.low <= scripData.dayLow) ||
                    (candleToDayLowRatio <= 0.1))) {
                scripData.DayLowReversalIdentified = true;
            }

            if (prevCandle.color == "Green" && currentCandle.color == "Red" &&
                ((currentCandle.high >= scripData.dayHigh) ||
                    (candleToDayHighRatio <= 0.1))) {
                scripData.DayHighReversalIdentified = true;
            }

            if (scripData.DayHighReversalIdentified ||
                scripData.DayLowReversalIdentified) {
                scripData.signalCandleHigh = (currentCandle.high > scripData.dayHigh) ? currentCandle.high : scripData.dayHigh;
                scripData.signalCandleLow = (currentCandle.low < scripData.dayLow) ? currentCandle.low : scripData.dayLow;
                Logger::getInstance().log(Logger::DEBUG, "***** Pattern Identified *****");

                OrderManager::getInstance().startOrderMonitoring(instrumentToken, scripData.signalCandleHigh,scripData.signalCandleLow);
            }
            //}
        }
    }
};
