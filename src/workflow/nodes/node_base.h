#pragma once

#include <string>
#include <any>
#include <map>
#include <halconcpp/HalconCpp.h>
#include "app/common.h"

struct NodeContext
{
    std::string camera_name;
    HalconCpp::HObject image;         // 原图（保持不变，用于存储）
    HalconCpp::HObject display_image; // 叠加检测结果的显示图（用于 UI）
    InspectionResult result;
    std::map<std::string, std::any> data;
};

class INode
{
public:
    virtual ~INode() = default;
    virtual std::string name() const = 0;
    virtual bool execute(NodeContext &ctx) = 0;
};
