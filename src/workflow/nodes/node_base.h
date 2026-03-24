#pragma once

#include <string>
#include <any>
#include <map>
#include <opencv2/core.hpp>
#include "app/common.h"

struct NodeContext
{
    std::string camera_id;
    cv::Mat image;
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
