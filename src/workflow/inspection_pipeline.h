#pragma once

#include "nodes/node_base.h"
#include "app/common.h"
#include <taskflow/taskflow.hpp>
#include <vector>
#include <memory>

class InspectionPipeline
{
public:
    explicit InspectionPipeline(const WorkflowParam &param);
    ~InspectionPipeline();

    /// @brief 根据 WorkflowParam 构建节点链
    void build();

    /// @brief 设置离线图像，下次 capture() 将使用此图像而非相机采集
    void setOfflineImage(const HalconCpp::HObject &image);

    /// @brief 清除离线图像，恢复相机采集模式
    void clearOfflineImage();

    /// @brief 是否处于离线模式
    bool isOffline() const { return offline_image_.IsInitialized(); }

    /// @brief 阶段1: 采集图像（同步）
    ///        若有离线图像则直接使用，否则从相机采集
    ///        成功后 ctx_ 中有 image + display_image
    bool capture();

    /// @brief 阶段2: 执行算法链 + 结果节点（同步）
    ///        在 display_image 上叠加检测框
    InspectionResult process(tf::Executor &executor);

    /// @brief 当前上下文（采集后可访问 image，检测后可访问 display_image）
    const NodeContext &context() const { return ctx_; }

    const WorkflowParam &param() const { return param_; }

private:
    WorkflowParam param_;
    tf::Taskflow taskflow_;
    NodeContext ctx_;

    std::unique_ptr<INode> capture_node_;                // 采集节点（单独执行）
    std::vector<std::unique_ptr<INode>> process_nodes_;  // 算法节点 + 结果节点（DAG执行）
    HalconCpp::HObject offline_image_;                   // 离线图像（非空时 capture() 跳过相机）
};
