/*
 * ============================================================================
 * ARC System Audio Host - Logger Implementation
 * ============================================================================
 */

#include "Logger.h"
#include <iostream>
#include <iomanip>

namespace ARC
{

//=============================================================================
std::unique_ptr<Logger> Logger::instance;
std::mutex Logger::instanceMutex;

//=============================================================================
Logger::Logger()
{
    // Default log file location
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("ARC System Audio Host")
                          .getChildFile("Logs");
    appDataDir.createDirectory();

    logFile = appDataDir.getChildFile("arc.log");
}

Logger::~Logger()
{
    if (fileStream.is_open())
    {
        fileStream.close();
    }
}

//=============================================================================
Logger& Logger::getInstance()
{
    std::lock_guard<std::mutex> lock(instanceMutex);

    if (instance == nullptr)
    {
        instance = std::unique_ptr<Logger>(new Logger());
    }

    return *instance;
}

//=============================================================================
void Logger::initialize(const juce::File& file)
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);

    logger.logFile = file;
    logger.logFile.getParentDirectory().createDirectory();

    if (logger.fileStream.is_open())
    {
        logger.fileStream.close();
    }

    logger.fileStream.open(file.getFullPathName().toStdString(), std::ios::app);
    logger.initialized = true;

    // Log startup message
    logger.logInternal("=== ARC System Audio Host Log Started ===", LogLevel::Info);
    logger.logInternal("Log file: " + file.getFullPathName(), LogLevel::Info);
}

void Logger::shutdown()
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);

    logger.logInternal("=== ARC System Audio Host Log Ended ===", LogLevel::Info);

    if (logger.fileStream.is_open())
    {
        logger.fileStream.close();
    }

    logger.initialized = false;
}

//=============================================================================
void Logger::log(const juce::String& message, LogLevel level)
{
    getInstance().logInternal(message, level);
}

void Logger::trace(const juce::String& message)
{
    log(message, LogLevel::Trace);
}

void Logger::debug(const juce::String& message)
{
    log(message, LogLevel::Debug);
}

void Logger::info(const juce::String& message)
{
    log(message, LogLevel::Info);
}

void Logger::warning(const juce::String& message)
{
    log(message, LogLevel::Warning);
}

void Logger::error(const juce::String& message)
{
    log(message, LogLevel::Error);
}

void Logger::fatal(const juce::String& message)
{
    log(message, LogLevel::Fatal);
}

//=============================================================================
void Logger::setLogLevel(LogLevel level)
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);
    logger.currentLevel = level;
}

void Logger::setLogToConsole(bool enabled)
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);
    logger.logToConsole = enabled;
}

void Logger::setLogToFile(bool enabled)
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);
    logger.logToFile = enabled;
}

void Logger::setMaxLogFileSize(int64_t bytes)
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);
    logger.maxLogSize = bytes;
}

//=============================================================================
void Logger::rotateLogFile()
{
    auto& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.logMutex);

    if (logger.fileStream.is_open())
    {
        logger.fileStream.close();
    }

    // Rotate existing log files
    for (int i = logger.maxLogFiles - 1; i >= 1; --i)
    {
        juce::File oldFile = logger.logFile.getSiblingFile(
            logger.logFile.getFileNameWithoutExtension() + "." + juce::String(i) +
            logger.logFile.getFileExtension());

        juce::File newFile = logger.logFile.getSiblingFile(
            logger.logFile.getFileNameWithoutExtension() + "." + juce::String(i + 1) +
            logger.logFile.getFileExtension());

        if (oldFile.existsAsFile())
        {
            if (i == logger.maxLogFiles - 1)
            {
                oldFile.deleteFile();
            }
            else
            {
                oldFile.moveFileTo(newFile);
            }
        }
    }

    // Move current log to .1
    juce::File rotatedFile = logger.logFile.getSiblingFile(
        logger.logFile.getFileNameWithoutExtension() + ".1" +
        logger.logFile.getFileExtension());

    logger.logFile.moveFileTo(rotatedFile);

    // Open new log file
    logger.fileStream.open(logger.logFile.getFullPathName().toStdString(), std::ios::app);
}

//=============================================================================
void Logger::logInternal(const juce::String& message, LogLevel level)
{
    std::lock_guard<std::mutex> lock(logMutex);

    if (level < currentLevel)
        return;

    juce::String formattedMessage = formatMessage(message, level);

    // Log to console
    if (logToConsole)
    {
        std::cout << formattedMessage.toStdString() << std::endl;
    }

    // Log to file
    if (logToFile && fileStream.is_open())
    {
        fileStream << formattedMessage.toStdString() << std::endl;
        fileStream.flush();

        checkRotation();
    }

    // JUCE logging
    juce::Logger::writeToLog(formattedMessage);
}

juce::String Logger::formatMessage(const juce::String& message, LogLevel level) const
{
    auto now = juce::Time::getCurrentTime();

    juce::String timestamp = now.formatted("%Y-%m-%d %H:%M:%S");

    return "[" + timestamp + "] [" + levelToString(level) + "] " + message;
}

juce::String Logger::levelToString(LogLevel level) const
{
    switch (level)
    {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "?????";
    }
}

void Logger::checkRotation()
{
    if (logFile.getSize() > maxLogSize)
    {
        // Close and reopen after rotation
        fileStream.close();
        rotateLogFile();
    }
}

} // namespace ARC

