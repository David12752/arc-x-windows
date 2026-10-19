/*
 * ============================================================================
 * ARC System Audio Host - Logger Header
 * Centralized logging system
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <mutex>
#include <fstream>
#include <memory>

namespace ARC
{

//=============================================================================
/**
 * Log levels
 */
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

//=============================================================================
/**
 * Logger class - singleton pattern
 * Thread-safe logging to file and console
 */
class Logger
{
public:
    //=========================================================================
    // Singleton access
    static Logger& getInstance();

    //=========================================================================
    // Initialize with log file
    static void initialize(const juce::File& logFile);
    static void shutdown();

    //=========================================================================
    // Logging methods
    static void log(const juce::String& message, LogLevel level = LogLevel::Info);
    static void trace(const juce::String& message);
    static void debug(const juce::String& message);
    static void info(const juce::String& message);
    static void warning(const juce::String& message);
    static void error(const juce::String& message);
    static void fatal(const juce::String& message);

    //=========================================================================
    // Configuration
    static void setLogLevel(LogLevel level);
    static void setLogToConsole(bool enabled);
    static void setLogToFile(bool enabled);
    static void setMaxLogFileSize(int64_t bytes);

    //=========================================================================
    // Log rotation
    static void rotateLogFile();

public:
    ~Logger();

private:
    //=========================================================================
    Logger();

    void logInternal(const juce::String& message, LogLevel level);
    juce::String formatMessage(const juce::String& message, LogLevel level) const;
    juce::String levelToString(LogLevel level) const;
    void checkRotation();

    //=========================================================================
    static std::unique_ptr<Logger> instance;
    static std::mutex instanceMutex;

    juce::File logFile;
    std::ofstream fileStream;
    std::mutex logMutex;

    LogLevel currentLevel{LogLevel::Info};
    bool logToConsole{true};
    bool logToFile{true};
    int64_t maxLogSize{10 * 1024 * 1024}; // 10 MB
    int maxLogFiles{5};

    bool initialized{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Logger)
};

//=============================================================================
// Convenience macros
#define ARC_LOG(msg) ARC::Logger::log(msg)
#define ARC_LOG_TRACE(msg) ARC::Logger::trace(msg)
#define ARC_LOG_DEBUG(msg) ARC::Logger::debug(msg)
#define ARC_LOG_INFO(msg) ARC::Logger::info(msg)
#define ARC_LOG_WARNING(msg) ARC::Logger::warning(msg)
#define ARC_LOG_ERROR(msg) ARC::Logger::error(msg)
#define ARC_LOG_FATAL(msg) ARC::Logger::fatal(msg)

} // namespace ARC

