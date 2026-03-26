#include "config_manager.h"
#include "app_context.h"
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

ConfigManager &ConfigManager::getInstance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager()
{
    loadConfig();
    // SPDLOG_INFO("Config loaded");
}

ConfigManager::~ConfigManager()
{
    saveConfig();
    // SPDLOG_INFO("Config saved");
}

bool ConfigManager::loadConfig(const std::string &path)
{
    return true;
}

bool ConfigManager::saveConfig(const std::string &path)
{
    return true;
}
