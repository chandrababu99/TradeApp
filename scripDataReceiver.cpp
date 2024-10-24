#include "Logger.h"
#include "Types.h"
#include "candleProcessor.cpp"
#include <fstream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <unordered_map>

class ScripDataReceiver {
  private:
    kc::kite* Kite;
    kc::ticker* Ticker;
    std::string accessToken;
    std::thread tickProcessingThread;

  public:
    ScripDataReceiver(const std::string& apiKey, const std::string& apiSecret, const std::string& reqToken) {
        try {
            Logger::getInstance().log(Logger::DEBUG, "Application started.");
                 //           tickProcessingThread =
                //std::thread(&ScripDataReceiver::processTicksInThread, this);
            ///*
                Kite = new kc::kite(apiKey);
                Ticker = new kc::ticker(apiKey, 5, true, 5);

                std::cout << "login URL: " << Kite->loginURL() << '\n';
                std::cout << "login with this URL and obtain the request token\n";

                accessToken = Kite->generateSession(reqToken,apiSecret).tokens.accessToken; 
                Kite->setAccessToken(accessToken);
                std::cout << "access token is " << Kite->getAccessToken() << '\n';

                kc::userProfile profile = Kite->profile();
                //logger.log(Logger::INFO, "name: ", profile.userName,"\n");
                //logger.log(Logger::INFO, "email: ", profile.email, "\n");

                Ticker->setAccessToken(accessToken);

                tickProcessingThread =
                std::thread(&ScripDataReceiver::processTicksInThread, this);

                Ticker->onConnect = [this](kc::ticker* ws) {
                    this->onConnect(ws); 
                }; 
                Ticker->onTicks = [this](kc::ticker* ws, const std::vector<kc::tick>&ticks) { 
                    this->onTicks(ws, ticks);        
                };
                Ticker->onError = [this](kc::ticker* ws, int code,
                                              const std::string& message) {
                            this->onError(ws, code, message);
                };
                Ticker->onConnectError = [this](kc::ticker* ws) {
                            this->onConnectError(ws);
                };
                Ticker->onClose = [this](kc::ticker* ws, int code,
                                              const std::string& message) {
                    this->onClose(ws, code, message);
                };
                Ticker->connect();
                Ticker->run();
                     // */  

        } catch (kc::kiteppException& e) {
            std::cerr << e.what() << ", " << e.code() << ", " << e.message()
                      << '\n';
        } catch (kc::libException& e) {
            std::cerr << e.what() << '\n';
        } catch (std::exception& e) { std::cerr << e.what() << std::endl; };
    }

    ~ScripDataReceiver() {
        Ticker->stop();
        tickProcessingThread.join();
        delete Ticker;
        delete Kite;
    }

    void onConnect(kc::ticker* ws) {
        std::cout << "connected.. Subscribing now..\n";
        ws->setMode("full", { 256265, 260105 });
    };
    void onTicks(kc::ticker*, const std::vector<kc::tick>& ticks) {
        // Forward the ticks to CandleProcessor for candle formation
        CandleProcessor::getInstance().addTicks(ticks);

        // Forward the same ticks to OrderManager for trade monitoring
        OrderManager::getInstance().updateTickData(ticks);
    }

    void onError(kc::ticker* ws, int code, const std::string& message) {
        std::cout << "Error! Code: " << code << " message: " << message << "\n";
    };

    void onConnectError(kc::ticker* ws) {
        std::cout << "Couldn't connect..\n";
    };

    void onClose(kc::ticker* ws, int code, const std::string& message) {
        std::cout << "Closed the connection.. code: " << code
                  << " message: " << message << "\n";
    };

    void processTicksInThread() {
        while (true) {
            // Process tick data every second in this thread
            //Logger::getInstance().log(Logger::DEBUG, "processTicksInThread: started ");

            CandleProcessor::getInstance().processTicks();

            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
    }
};

// Function to generate random ticks
std::vector<kc::tick> generateRandomTicks(int numTicks) {
    std::vector<kc::tick> ticks;
    ticks.reserve(numTicks);

    int32_t minPrice = 25200;
    int32_t maxPrice = 24500;

    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    for (int i = 0; i < numTicks; ++i) {
        kc::tick newTick;
        newTick.instrumentToken = 100000+i;
        newTick.lastPrice = minPrice + std::rand() % (maxPrice - minPrice + 1);
        ticks.push_back(newTick);
    }

    return ticks;
}

int main(int argc, char* argv[]) {

    std::ifstream file("user_info.json");
    if (!file.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return 1;
    }

    nlohmann::json jsonData;
    file >> jsonData;

    if (jsonData.empty()) {
        std::cerr << "Input data in user_data.json is empty!" << std::endl;
        return 1;
    }

    auto apiKey = jsonData[0]["api_key"];
    auto apiSecret = jsonData[0]["api_secret"];
    auto reqToken = jsonData[0]["auth_token"];

    ScripDataReceiver receiver(apiKey, apiSecret, reqToken);
    Logger::getInstance().setLogLevel(Logger::DEBUG);
  /*  
    auto j = 1;

    while (j < 1505) {
        //Logger::getInstance().log(Logger::DEBUG, "J now : ", j);
        std::vector<kc::tick> tickmap = generateRandomTicks(2);

        CandleProcessor::getInstance().addTicks(tickmap);
        OrderManager::getInstance().updateTickData(tickmap);

        tickmap.clear();
        j++;

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
*/
    return 0;
};
