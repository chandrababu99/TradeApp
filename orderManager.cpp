
#include "Types.h"
#include "Logger.h"
#include <mutex>
#include <condition_variable>
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

    void updateTickData(const std::vector<kc::tick>& ticks) {
        std::lock_guard<std::mutex> lock(orderMutex);
        for (const auto& tick : ticks) {
            latestTickData[tick.instrumentToken] = tick;
        }
        cv.notify_all();
    }
    void updateCandleData(const double& instrumentToken, ScripData& scripData) {
        std::lock_guard<std::mutex> lock(scripDataMutex);
        OrderscripDataMap[instrumentToken] = scripData;
        sdMutex_cv.notify_all();
    }
    void startOrderMonitoring(const double& instrumentToken, ScripData& scripData) {
        Logger::getInstance().log(Logger::DEBUG, "startOrderMonitoring for ", instrumentToken);
        orderMonitoringThreads[instrumentToken] = std::thread(
            &OrderManager::monitorForTrade, this, instrumentToken, scripData);
    }
    void monitorForTrade(const double& instrumentToken, const ScripData& scripData) {
        bool tradeExecuted = false;
        //TO DO Entry can be 0.1% above/below the signal candle.
        while (!tradeExecuted && !stopMonitoring) {
            double currentPrice = getCurrentPrice(instrumentToken);
            if (currentPrice > scripData.signalCandleHigh) {
            //Buy Call 
            // placeBuyOrder(instrumentToken, currentPrice, "CE");
            tradeExecuted = true;
            Logger::getInstance().log(Logger::DEBUG, "***** Trade Executed for CE ", instrumentToken, "at price", currentPrice);


            } else if (currentPrice < scripData.signalCandleLow) {
                // Buy put
            // placeBuyOrder(instrumentToken, currentPrice, "PE");
            //    startSellMonitoring(instrumentToken, stopLoss);
            tradeExecuted = true;
            Logger::getInstance().log(Logger::DEBUG, "***** Trade Executed for PE", instrumentToken, "at price", currentPrice);

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

    void startSellMonitoring(const double& instrumentToken, double stopLoss) {
        std::thread([this, instrumentToken, stopLoss] {
            while (true) {
                double currentPrice = getCurrentPrice(instrumentToken);

                if (currentPrice <= stopLoss) {
                    //placeSellOrder(instrumentToken, currentPrice);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
            }
        }).detach(); // Detached thread to handle sell monitoring asynchronously
    }

    void startCallExitMonitoring(const double& instrumentToken, double& stopLoss){
            std::thread([this, instrumentToken, stopLoss] {
            while (true) {
                double currentPrice = getCurrentPrice(instrumentToken);

                if (currentPrice < stopLoss) {
                    //placeSellOrder(instrumentToken, currentPrice);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
            }
        }).detach();

    }
    void startPutExitMonitoring(const double& instrumentToken, double& stopLoss){}

    void placeBuyOrder(const double& instrumentToken, const double& buyPrice, const std::string& OpType ) {
        std::lock_guard<std::mutex> lock(orderMutex);
        // Place buy order logic
    }

    void placeSellOrder(const double& instrumentToken, const double& sellPrice, const std::string Optype) {
        std::lock_guard<std::mutex> lock(orderMutex);
        // Place sell order logic (based on stop loss or target)
    }

};
