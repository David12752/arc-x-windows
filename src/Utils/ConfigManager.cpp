/*
 * ============================================================================
 * ARC System Audio Host - Config Manager Implementation
 * ============================================================================
 */

#include "ConfigManager.h"
#include "Logger.h"

namespace ARC
{

//=============================================================================
ConfigManager::ConfigManager()
{
    // Default config directory
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("ARC System Audio Host");
    appDataDir.createDirectory();

    configFile = appDataDir.getChildFile("settings.xml");

    // Set up options
    options = std::make_unique<juce::PropertiesFile::Options>();
    options->applicationName = "ARC System Audio Host";
    options->filenameSuffix = ".xml";
    options->folderName = "ARC System Audio Host";
    options->storageFormat = juce::PropertiesFile::storeAsXML;

    // Create properties file
    properties = std::make_unique<juce::PropertiesFile>(*options);

    Logger::log("ConfigManager created - file: " + configFile.getFullPathName());
}

ConfigManager::~ConfigManager()
{
    save();
    Logger::log("ConfigManager destroyed");
}

//=============================================================================
void ConfigManager::initialize(const juce::File& file)
{
    std::lock_guard<std::mutex> lock(mutex);

    configFile = file;
    load();
}

//=============================================================================
void ConfigManager::save()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->saveIfNeeded();
        Logger::log("Configuration saved");
    }
}

void ConfigManager::load()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (configFile.existsAsFile())
    {
        properties = std::make_unique<juce::PropertiesFile>(configFile, *options);
        Logger::log("Configuration loaded from: " + configFile.getFullPathName());
    }
    else
    {
        Logger::log("No configuration file found, using defaults");
    }
}

void ConfigManager::reset()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->clear();
        Logger::log("Configuration reset to defaults");
    }
}

//=============================================================================
bool ConfigManager::getValue(const juce::String& key, bool defaultValue) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->getBoolValue(key, defaultValue);
    }
    return defaultValue;
}

int ConfigManager::getValue(const juce::String& key, int defaultValue) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->getIntValue(key, defaultValue);
    }
    return defaultValue;
}

double ConfigManager::getValue(const juce::String& key, double defaultValue) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->getDoubleValue(key, defaultValue);
    }
    return defaultValue;
}

juce::String ConfigManager::getValue(const juce::String& key, const juce::String& defaultValue) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->getValue(key, defaultValue);
    }
    return defaultValue;
}

juce::StringArray ConfigManager::getValue(const juce::String& key,
                                          const juce::StringArray& defaultValue) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        juce::String value = properties->getValue(key);
        if (value.isNotEmpty())
        {
            juce::StringArray result;
            result.addTokens(value, ";", "\"");
            return result;
        }
    }
    return defaultValue;
}

//=============================================================================
void ConfigManager::setValue(const juce::String& key, bool value)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->setValue(key, value);
    }
}

void ConfigManager::setValue(const juce::String& key, int value)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->setValue(key, value);
    }
}

void ConfigManager::setValue(const juce::String& key, double value)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->setValue(key, value);
    }
}

void ConfigManager::setValue(const juce::String& key, const juce::String& value)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->setValue(key, value);
    }
}

void ConfigManager::setValue(const juce::String& key, const juce::StringArray& value)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->setValue(key, value.joinIntoString(";"));
    }
}

//=============================================================================
bool ConfigManager::hasKey(const juce::String& key) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->containsKey(key);
    }
    return false;
}

void ConfigManager::removeKey(const juce::String& key)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        properties->removeValue(key);
    }
}

juce::StringArray ConfigManager::getAllKeys() const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        return properties->getAllProperties().getAllKeys();
    }
    return {};
}

//=============================================================================
juce::File ConfigManager::getConfigDirectory() const
{
    return configFile.getParentDirectory();
}

//=============================================================================
bool ConfigManager::exportConfig(const juce::File& file) const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (properties != nullptr)
    {
        auto xml = properties->createXml("ARCConfig");
        if (xml != nullptr)
        {
            return xml->writeTo(file);
        }
    }
    return false;
}

bool ConfigManager::importConfig(const juce::File& file)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr && properties != nullptr)
    {
        for (auto* element : xml->getChildIterator())
        {
            juce::String key = element->getTagName();
            juce::String value = element->getAllSubText();
            properties->setValue(key, value);
        }
        return true;
    }
    return false;
}

} // namespace ARC

