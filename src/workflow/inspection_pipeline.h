#pragma once

#include "algorithm/algorithm_interface.h"
#include "app/common.h"
#include <vector>
#include <memory>
#include <halconcpp/HalconCpp.h>

class InspectionPipeline
{
public:
    explicit InspectionPipeline(const WorkflowParam &param);
    ~InspectionPipeline();

    /// 根据 WorkflowParam 创建算法实例
    void build();

    void setOfflineImage(const HalconCpp::HObject &image);
    void clearOfflineImage();
    bool isOffline() const { return offline_image_.IsInitialized(); }

    /// 采集图像（离线时使用注入图像，在线时从相机采集）
    bool capture();

    /// 顺序执行算法链 + 打时间戳
    InspectionResult process();

    const NodeContext &context() const { return ctx_; }
    const WorkflowParam &param() const { return param_; }

    /// 获取算法列表（供 UI 配置用）
    std::vector<IAlgorithm *> algorithms();

private:
    WorkflowParam param_;
    NodeContext ctx_;
    std::vector<std::unique_ptr<IAlgorithm>> algorithms_;
    HalconCpp::HObject offline_image_;
};
