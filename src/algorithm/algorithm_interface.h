#pragma once

#include <string>
#include <halconcpp/HalconCpp.h>
#include "app/common.h"
#include <nlohmann/json.hpp>

/// 算法分类常量
namespace AlgorithmCategory
{
    constexpr const char *IMAGE_PROCESSING = "图像处理";
    constexpr const char *DETECTION = "检测识别";
    constexpr const char *CALIBRATION = "标定工具";
    constexpr const char *MEASUREMENT = "几何测量";
    constexpr const char *AI_INFER = "AI推理";
}

class IAlgorithm
{
public:
    virtual ~IAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual std::string category() const = 0;
    virtual std::string description() const = 0;

    virtual bool init(const nlohmann::json &params) = 0;
    virtual bool process(const HalconCpp::HObject &input, InspectionResult &result) = 0;

    virtual nlohmann::json defaultParams() const = 0;
};
