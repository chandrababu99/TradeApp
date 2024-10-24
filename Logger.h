#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>

class Logger {
public:
    // Enum for log levels (optional)
    enum LogLevel { INFO, DEBUG, ERROR };

    // Get the singleton instance of Logger
    static Logger& getInstance();

    // Set the logging level threshold
    void setLogLevel(LogLevel level);

    // API to log messages (move definition into the header)
    template <typename... Args>
    void log(LogLevel level, Args... args);

    // Destructor to gracefully shut down the logging thread
    ~Logger();

private:
    // Private constructor for singleton
    Logger();

    // Method for the logging thread to process log entries
    void processLogs();

    // Convert different types of arguments to string
    template <typename T>
    std::string toString(const T& value);

    // Handle the log level as a string
    std::string logLevelToString(LogLevel level);

    // Logger data members
    std::ofstream logFile;
    std::queue<std::string> logQueue;
    std::mutex queueMutex;
    std::condition_variable logCondition;
    bool isRunning;
    std::thread logThread;
    LogLevel logLevelThreshold;  // The log level threshold

    // Constants
    const int BUFFER_FLUSH_LIMIT = 10;  // Max number of logs before flush
};

// Singleton instance
inline Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

inline Logger::Logger() : isRunning(true), logLevelThreshold(DEBUG) {  // Default level is DEBUG
    logFile.open("trade_data.log", std::ios::out | std::ios::app);
    logThread = std::thread(&Logger::processLogs, this);
}

// Destructor to stop the logging thread
inline Logger::~Logger() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        isRunning = false;
    }
    logCondition.notify_all();
    logThread.join();
    logFile.close();
}

// Set the logging level threshold
inline void Logger::setLogLevel(LogLevel level) {
    logLevelThreshold = level;
}

// Variadic template to accept different types of arguments
template <typename... Args>
inline void Logger::log(LogLevel level, Args... args) {
    // Only log if the message level is >= logLevelThreshold
    if (level >= logLevelThreshold) {
        std::stringstream logStream;
                // Get current time
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* local_time = std::localtime(&now_time);

        // Add the timestamp to the log
        logStream << "[" << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << "] ";

        logStream << "[" << logLevelToString(level) << "] ";
        (logStream << ... << toString(args)) << "\n";  // Fold expression (C++17)

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            logQueue.push(logStream.str());
        }
        logCondition.notify_one();
    }
}

template <typename T>
inline std::string Logger::toString(const T& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

inline std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case INFO: return "INFO";
        case DEBUG: return "DEBUG";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Background logging thread function
inline void Logger::processLogs() {
    while (isRunning) {
        std::unique_lock<std::mutex> lock(queueMutex);
        logCondition.wait_for(lock, std::chrono::seconds(2), [this]() { return !logQueue.empty() || !isRunning; });

        while (!logQueue.empty()) {
            logFile << logQueue.front();
            logQueue.pop();
        }
        logFile.flush();
    }
}

#endif // LOGGER_H
