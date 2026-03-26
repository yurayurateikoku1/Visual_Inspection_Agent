#pragma once

#include <nlohmann/json.hpp>
#include <string>

/// @brief 配置管理器：纯文件 I/O，负责 JSON 配置的加载/保存
///        加载时将数据填充到 AppContext，保存时从 AppContext 读取
class ConfigManager
{
public:
    static ConfigManager &getInstance();
    ~ConfigManager();

    /// @brief 从 JSON 文件加载配置到 AppContext
    bool loadConfig(const std::string &path = "data/config.json");

    /// @brief 从 AppContext 读取配置保存到 JSON 文件
    bool saveConfig(const std::string &path = "data/config.json");

private:
    ConfigManager();
};
