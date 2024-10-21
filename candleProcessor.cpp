#include "Logger.h"
#include "Types.h"
#include "patternDetector.cpp"

#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <unordered_map>

// CandleProcessor handles tick data and creates 15-minute candles
class CandleProcessor {
  private:
    std::unordered_map<double, ScripData> scripDataMap;
    std::unordered_map<double,
        std::chrono::time_point<std::chrono::system_clock>>
        lastCandleTimes;
    std::mutex candleMutex;
    std::condition_variable tickCondition;
    std::vector<kc::tick> tickQueue;

    CandleProcessor() {} // Singleton pattern

  public:
    static CandleProcessor& getInstance() {
        static CandleProcessor instance;
        return instance;
    }

    void addTicks(const std::vector<kc::tick>& ticks) {
        Logger::getInstance().log(
            Logger::DEBUG, "addTicks: started ", ticks.size(), " ticks");

        std::lock_guard<std::mutex> lock(candleMutex);
        tickQueue.insert(tickQueue.end(), ticks.begin(), ticks.end());

        tickCondition.notify_one();
    }

    void processTicks() {
        // Logger::getInstance().log(Logger::DEBUG, "Process Ticks: started ");

        std::vector<kc::tick> ticksToProcess;
        {
            std::unique_lock<std::mutex> lock(candleMutex);
            tickCondition.wait(lock, [this] { return !tickQueue.empty(); });
            ticksToProcess.swap(tickQueue);
        }
       // Logger::getInstance().log(
         //   Logger::DEBUG, "Process Ticks: Working on local copy ");

        for (const auto& tick : ticksToProcess) {
            updateCandle(tick.instrumentToken, tick.lastPrice);
        }
    }

    void updateCandle(const double& instrumentToken, const double& lastPrice) {
        Logger::getInstance().log(Logger::DEBUG, "Update Candle : started ");

        auto currentTime = std::chrono::system_clock::now();
        auto endTime = getCandleEndTime(currentTime);
        // Check if this instrument is being processed for the first time
        if (lastCandleTimes.find(instrumentToken) == lastCandleTimes.end()) {
            Candle newCandle = { lastPrice, lastPrice, lastPrice, lastPrice,
                "Green", currentTime, endTime };
            ScripData scripData = { { newCandle }, lastPrice, lastPrice };
            // scripDataMap[instrumentToken] = scripData;
            scripDataMap.emplace(instrumentToken, scripData);
            lastCandleTimes[instrumentToken] = currentTime;
            return;
        }

        auto& currentCandle = scripDataMap[instrumentToken].candles.back();
        if (currentCandle.endTime <= currentCandle.startTime) {
            finalizeCandle(instrumentToken, lastPrice);
        } else {

            currentCandle.high = std::max(currentCandle.high, lastPrice);
            currentCandle.low = std::min(currentCandle.low, lastPrice);
            currentCandle.close = lastPrice;
        }

        updateDayHighLow(instrumentToken, lastPrice);
    }

    void finalizeCandle(
        const double& instrumentToken, const double& lastPrice) {
        auto& scripData = scripDataMap[instrumentToken];
        Candle& lastCandle = scripData.candles.back();
        lastCandle.close = lastPrice;
        lastCandle.color =
            (lastCandle.open < lastCandle.close) ? "Green" : "Red";
        auto candleSize = (lastCandle.high - lastCandle.low);
        lastCandle.bodyRatio =
            (std::abs(lastCandle.open - lastCandle.close) / candleSize) * 100;
        lastCandle.wickRatio = 100 - lastCandle.bodyRatio;
        lastCandle.candleToIndexRatio = (candleSize / lastCandle.high) * 100;

        // Logging the candle
        Logger::getInstance().log(Logger::DEBUG, "Scrip: ", instrumentToken,
            " | Open: ", lastCandle.open, ", High: ", lastCandle.high,
            ", Low: ", lastCandle.low, ", Close: ", lastCandle.close, "\n",
            " | Candle Color: ", lastCandle.color,
            " | Day High: ", scripData.dayHigh, ", Day Low: ", scripData.dayLow,
            "\n", " | bodyRatio: ", lastCandle.bodyRatio,
            ", candleToIndexRatio: ", lastCandle.candleToIndexRatio);
        auto timenow = std::chrono::system_clock::to_time_t(lastCandle.endTime);
        Logger::getInstance().log(Logger::DEBUG, ", Time : ", ctime(&timenow));

        if (!(scripData.DayHighReversalIdentified == true ||
                scripData.DayLowReversalIdentified == true)) {
            PatternDetector::getInstance().detectPattern(instrumentToken, scripData);
        }
        OrderManager::getInstance().updateCandleData(instrumentToken, scripData);

        // Start a new candle
        auto currentTime = std::chrono::system_clock::now();
        Candle newCandle = { lastPrice, lastPrice, lastPrice, lastPrice,
            "Green", currentTime, getCandleEndTime(currentTime) };
        scripData.candles.push_back(newCandle);

        // Keep only the last two candles
        if (scripData.candles.size() > 2) {
            scripData.candles.erase(scripData.candles.begin());
        }

        // Update the last candle time for this scrip
        lastCandleTimes[instrumentToken] = std::chrono::system_clock::now();
    }

    void updateDayHighLow(
        const double& instrumentToken, const double& lastPrice) {
        auto& scripData = scripDataMap[instrumentToken];
        scripData.dayHigh = std::max(scripData.dayHigh, lastPrice);
        scripData.dayLow = std::min(scripData.dayLow, lastPrice);
    }

    std::chrono::system_clock::time_point getCandleEndTime(
        const std::chrono::system_clock::time_point& tick_time) {
        time_t raw_time = std::chrono::system_clock::to_time_t(tick_time);
        std::tm* time_info = std::localtime(&raw_time);

        // Align to nearest 15-minute interval
        int minutes = time_info->tm_min;
        int remainder = minutes % 15;
        time_info->tm_min -= remainder;
        if (remainder > 0) { time_info->tm_min += 15; }
        // Set seconds to zero
        time_info->tm_sec = 0;
        if (time_info->tm_min == 60) {
            time_info->tm_min = 0;
            time_info->tm_hour++;
        }
        return std::chrono::system_clock::from_time_t(std::mktime(time_info));
    }
};