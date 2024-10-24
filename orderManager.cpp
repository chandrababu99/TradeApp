
#include "Logger.h"
#include "Types.h"
#include <condition_variable>
#include <mutex>
#include <unordered_map>

// OrderManager handles placing buy and sell orders based on detected patterns
class OrderManager {
  private:
    std::mutex orderMutex;
    std::unordered_map<double, std::thread> orderMonitoringThreads;
    std::unordered_map<double, kc::tick> latestTickData;
    std::condition_variable cv;
    bool stopMonitoring = false;
    std::unordered_map<double, ScripData> OrderscripDataMap;
    std::mutex scripDataMutex;
    std::condition_variable sdMutex_cv;

    OrderManager() {}

  public:
    static OrderManager& getInstance() {
        static OrderManager instance;
        return instance;
    }
    ~OrderManager() {
        /*         for (auto& thread : orderMonitoringThreads) {
                   if (thread.joinable()) {
                   thread.join();
            }
        }*/
    }

    void updateTickData(const std::vector<kc::tick>& ticks) {
        //Logger::getInstance().log(Logger::DEBUG, " Inside updateTickData ");

        std::lock_guard<std::mutex> lock(orderMutex);
        for (const auto& tick : ticks) {
            latestTickData[tick.instrumentToken] = tick;
          //  Logger::getInstance().log(Logger::DEBUG, " Inside updateTickData  *", tick.lastPrice);
        }
        //cv.notify_all();
    }
    void updateCandleData(const double& instrumentToken, ScripData& scripData) {
        std::lock_guard<std::mutex> lock(scripDataMutex);
        OrderscripDataMap[instrumentToken] = scripData;
        sdMutex_cv.notify_all();
    }
    void startOrderMonitoring(
        const double& instrumentToken, const double signalCandleHigh, const double signalCandleLow) {
        Logger::getInstance().log(
            Logger::DEBUG, "startOrderMonitoring for ", instrumentToken);

        orderMonitoringThreads[instrumentToken] = std::thread(
            &OrderManager::monitorForTrade, this, instrumentToken, signalCandleHigh, signalCandleLow);
    }
    void monitorForTrade(
        const double& instrumentToken, const double signalCandleHigh, const double signalCandleLow) {
        bool tradeExecuted = false;
        double stopLoss = 0;
        // TO DO Entry can be 0.1% above/below the signal candle.
        while (!tradeExecuted && !stopMonitoring) {
            double currentPrice = getCurrentPrice(instrumentToken);
            if (currentPrice > signalCandleHigh) {
                // Buy Call
                //  placeBuyOrder(instrumentToken, currentPrice, "CE");
                tradeExecuted = true;
                stopLoss = signalCandleLow;
                Logger::getInstance().log(Logger::DEBUG,
                    "***** Trade Executed for CE ", instrumentToken, " at price ",
                    currentPrice);

                auto currentTime = std::chrono::system_clock::now();
                time_t raw_time =
                    std::chrono::system_clock::to_time_t(currentTime);
                std::tm* time_info = std::localtime(&raw_time);

                // Align to nearest 15-minute interval
                int minutes = time_info->tm_min;
                int remainder = minutes % 15;
                auto minutesToWait = 30 - remainder;

                startCallExitMonitoring(
                    instrumentToken, stopLoss, currentTime, minutesToWait);

            } else if (currentPrice < signalCandleLow) {
                // Buy put
                tradeExecuted = true;
                stopLoss = signalCandleHigh;
                Logger::getInstance().log(Logger::DEBUG,
                    "***** Trade Executed for PE", instrumentToken, " at price ",
                    currentPrice);
                auto currentTime = std::chrono::system_clock::now();
                time_t raw_time =
                    std::chrono::system_clock::to_time_t(currentTime);
                std::tm* time_info = std::localtime(&raw_time);

                // Align to nearest 15-minute interval
                int minutes = time_info->tm_min;
                int remainder = minutes % 15;
                auto minutesToWait = 30 - remainder;

                startPutExitMonitoring(
                    instrumentToken, stopLoss, currentTime, minutesToWait);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    double getCurrentPrice(const double& instrumentToken) {
        std::lock_guard<std::mutex> lock(orderMutex);
        return latestTickData[instrumentToken].lastPrice;
    }
    ScripData& getCandleData(const double& instrumentToken) {
        std::lock_guard<std::mutex> lock(scripDataMutex);
        return OrderscripDataMap[instrumentToken];
    }

    void startCallExitMonitoring(const double& instrumentToken,
        double& stopLoss,
        std::chrono::_V2::system_clock::time_point& currentTime,
        int& minsToWait) {
        std::thread([this, instrumentToken, &stopLoss, currentTime,
                        minsToWait] {
            while (true) {
                double currentPrice = getCurrentPrice(instrumentToken);

                if (currentPrice < stopLoss) {
                    // placeSellOrder(instrumentToken, currentPrice);
                    Logger::getInstance().log(Logger::DEBUG,
                        "***** Exit CE after SL/Target hit ", instrumentToken,
                        " at price ", currentPrice);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                auto duration =
                    std::chrono::duration_cast<std::chrono::minutes>(
                        std::chrono::system_clock::now() - currentTime);
                if (duration.count() > minsToWait) {
                    auto scripData = getCandleData(instrumentToken);
                    Candle& currentCandle = scripData.candles.back();
                    if (currentCandle.color == "Red") {
                        stopLoss = currentCandle.low;
                    }
                }
            }
        }).detach();
    }
    void startPutExitMonitoring(const double& instrumentToken, double& stopLoss,
        std::chrono::_V2::system_clock::time_point& currentTime,
        int& minsToWait) {
        std::thread([this, instrumentToken, &stopLoss, currentTime,
                        minsToWait] {
            while (true) {
                double currentPrice = getCurrentPrice(instrumentToken);

                if (currentPrice > stopLoss) {
                    // placeSellOrder(instrumentToken, currentPrice);
                    Logger::getInstance().log(Logger::DEBUG,
                        "***** Exit PE after SL/Target hit ", instrumentToken,
                        " at price ", currentPrice);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                auto duration =
                    std::chrono::duration_cast<std::chrono::minutes>(
                        std::chrono::system_clock::now() - currentTime);
                if (duration.count() > minsToWait) {
                    auto scripData = getCandleData(instrumentToken);
                    Candle& currentCandle = scripData.candles.back();
                    if (currentCandle.color == "Green") {
                        stopLoss = currentCandle.high;
                    }
                }
            }
        }).detach();
    }

    void placeBuyOrder(const double& instrumentToken, const double& buyPrice,
        const std::string& OpType) {
        std::lock_guard<std::mutex> lock(orderMutex);
        // Place buy order logic
    }

    void placeSellOrder(const double& instrumentToken, const double& sellPrice,
        const std::string Optype) {
        std::lock_guard<std::mutex> lock(orderMutex);
        // Place sell order logic (based on stop loss or target)
    }
};
