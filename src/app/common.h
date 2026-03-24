#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <opencv2/core.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct InspectionResult
{
    bool pass = true;
    std::string detail;
    std::vector<cv::Rect> defect_regions;
    double confidence = 0.0;
    int64_t timestamp_ms = 0;
};

struct CameraConfig
{
    std::string id;
    std::string name;
    std::string ip;
    float exposure_time = 10000.0f;
    float gain = 0.0f;
    int trigger_mode = 0; // 0=连续采集, 1=软触发, 2=硬触发
    int rotation_deg = 0; // 0, 90, 180, 270
};

struct CommConfig
{
    std::string id;
    std::string name;
    std::string protocol; // "modbus_tcp"
    std::string ip;
    int port = 502;
};

struct WorkflowConfig
{
    std::string id;
    std::string name;
    std::string camera_id;
    std::vector<std::string> algorithm_ids;
    std::string comm_id;
};
