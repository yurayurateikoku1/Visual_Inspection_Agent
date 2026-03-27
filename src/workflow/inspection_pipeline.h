#pragma once

#include "app/common.h"
#include "ai_infer/yolo.h"
#include <memory>
#include <halconcpp/HalconCpp.h>

/// @brief 单条检测流水线：capture → ROI裁剪 → YOLO检测 → 时间戳
///        算法链固定，行为由 WorkflowParam 中的 enabled 标志控制
class InspectionPipeline
{
public:
    explicit InspectionPipeline(const WorkflowParam &param);
    ~InspectionPipeline();

    /// @brief 根据 WorkflowParam 初始化检测器（加载YOLO模型等）
    void build();

    void setOfflineImage(const HalconCpp::HObject &image);
    void clearOfflineImage();
    bool isOffline() const { return offline_image_.IsInitialized(); }

    /// @brief 采集图像（离线时使用注入图像，在线时从相机 grabOne）
    bool capture();

    /// @brief 执行固定算法链：ROI裁剪 → YOLO检测 → 打时间戳
    InspectionResult process();

    const NodeContext &context() const { return ctx_; }
    const WorkflowParam &param() const { return param_; }

private:
    void processRoi();
    void processYolo();

    WorkflowParam param_;
    NodeContext ctx_;
    HalconCpp::HObject offline_image_;

    std::unique_ptr<AIInfer::YoloDetector> detector_; // YOLO 检测器（懒加载）
};
