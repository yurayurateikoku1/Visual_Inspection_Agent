#include "config_manager.h"
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

ConfigManager &ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

bool ConfigManager::loadConfig(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        spdlog::warn("Config file not found: {}, using defaults", path);
        return false;
    }

    try
    {
        json cfg = json::parse(f);

        camera_configs_.clear();
        for (auto &c : cfg.value("cameras", json::array()))
        {
            CameraConfig cc;
            cc.id = c.value("id", "");
            cc.name = c.value("name", "");
            cc.ip = c.value("ip", "");
            cc.exposure_time = c.value("exposure_time", 10000.0f);
            cc.gain = c.value("gain", 0.0f);
            cc.trigger_mode = c.value("trigger_mode", 0);
            cc.rotation_deg = c.value("rotation_deg", 0);
            camera_configs_.push_back(cc);
        }

        comm_configs_.clear();
        for (auto &c : cfg.value("communications", json::array()))
        {
            CommConfig cc;
            cc.id = c.value("id", "");
            cc.name = c.value("name", "");
            cc.protocol = c.value("protocol", "modbus_tcp");
            cc.ip = c.value("ip", "");
            cc.port = c.value("port", 502);
            comm_configs_.push_back(cc);
        }

        workflow_configs_.clear();
        for (auto &w : cfg.value("workflows", json::array()))
        {
            WorkflowConfig wc;
            wc.id = w.value("id", "");
            wc.name = w.value("name", "");
            wc.camera_id = w.value("camera_id", "");
            wc.algorithm_ids = w.value("algorithm_ids", std::vector<std::string>{});
            wc.comm_id = w.value("comm_id", "");
            workflow_configs_.push_back(wc);
        }

        spdlog::info("Config loaded: {} cameras, {} comms, {} workflows",
                     camera_configs_.size(), comm_configs_.size(), workflow_configs_.size());
        return true;
    }
    catch (const json::exception &e)
    {
        spdlog::error("Config parse error: {}", e.what());
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string &path)
{
    json cfg;

    json cameras = json::array();
    for (auto &c : camera_configs_)
    {
        cameras.push_back({{"id", c.id}, {"name", c.name}, {"ip", c.ip}, {"exposure_time", c.exposure_time}, {"gain", c.gain}, {"trigger_mode", c.trigger_mode}, {"rotation_deg", c.rotation_deg}});
    }
    cfg["cameras"] = cameras;

    json comms = json::array();
    for (auto &c : comm_configs_)
    {
        comms.push_back({{"id", c.id}, {"name", c.name}, {"protocol", c.protocol}, {"ip", c.ip}, {"port", c.port}});
    }
    cfg["communications"] = comms;

    json workflows = json::array();
    for (auto &w : workflow_configs_)
    {
        workflows.push_back({{"id", w.id}, {"name", w.name}, {"camera_id", w.camera_id}, {"algorithm_ids", w.algorithm_ids}, {"comm_id", w.comm_id}});
    }
    cfg["workflows"] = workflows;

    std::ofstream f(path);
    if (!f.is_open())
    {
        spdlog::error("Failed to save config: {}", path);
        return false;
    }
    f << cfg.dump(4);
    spdlog::info("Config saved to {}", path);
    return true;
}
