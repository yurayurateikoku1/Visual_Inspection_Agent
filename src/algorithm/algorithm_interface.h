#pragma once

#include <string>
#include <halconcpp/HalconCpp.h>
#include "app/common.h"
#include <nlohmann/json.hpp>

class IAlgorithm
{
public:
    virtual ~IAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    virtual bool init(const nlohmann::json &params) = 0;
    virtual bool process(const HalconCpp::HObject &input, InspectionResult &result) = 0;

    virtual nlohmann::json defaultParams() const = 0;
};
