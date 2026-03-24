#pragma once

#include <string>
#include <opencv2/core.hpp>
#include "app/common.h"
#include <nlohmann/json.hpp>

class IAlgorithm
{
public:
    virtual ~IAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    virtual bool init(const nlohmann::json &params) = 0;
    virtual bool process(const cv::Mat &input, InspectionResult &result) = 0;

    virtual nlohmann::json defaultParams() const = 0;
};
