#pragma once

#include "common.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

/// @brief 配置管理器：负责 JSON 配置文件的加载/保存
class ConfigManager
{
public:
    static ConfigManager &instance();

    bool loadConfig(const std::string &path = "config.json");
    bool saveConfig(const std::string &path = "config.json");

    std::vector<CameraConfig> &cameraConfigs() { return camera_configs_; }
    std::vector<CommConfig> &commConfigs() { return comm_configs_; }
    std::vector<WorkflowConfig> &workflowConfigs() { return workflow_configs_; }

    const std::vector<CameraConfig> &cameraConfigs() const { return camera_configs_; }
    const std::vector<CommConfig> &commConfigs() const { return comm_configs_; }
    const std::vector<WorkflowConfig> &workflowConfigs() const { return workflow_configs_; }

private:
    ConfigManager() = default;

    std::vector<CameraConfig> camera_configs_;
    std::vector<CommConfig> comm_configs_;
    std::vector<WorkflowConfig> workflow_configs_;
};
