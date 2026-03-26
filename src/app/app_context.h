#pragma once

#include "common.h"
#include <taskflow/taskflow.hpp>
#include <vector>

/// @brief 全局运行时上下文：持有运行时参数和共享资源
///        ConfigManager 负责初始化加载和关闭时回写，AppContext 负责运行时持有
class AppContext
{
public:
    static AppContext &getInstance();

    // 运行时参数（各模块读写）

    std::vector<CameraParam> &cameraParams() { return camera_params_; }
    std::vector<CommunicationParam> &commParams() { return comm_params_; }
    LightParam &lightParam() { return light_param_; }
    std::vector<WorkflowParam> &workflowParams() { return workflow_params_; }

    const std::vector<CameraParam> &cameraParams() const { return camera_params_; }
    const std::vector<CommunicationParam> &commParams() const { return comm_params_; }
    const LightParam &lightParam() const { return light_param_; }
    const std::vector<WorkflowParam> &workflowParams() const { return workflow_params_; }

    // 共享任务池
    tf::Executor &executor() { return executor_; }

private:
    AppContext();
    ~AppContext() = default;

    std::vector<CameraParam> camera_params_;
    std::vector<CommunicationParam> comm_params_;
    LightParam light_param_;
    std::vector<WorkflowParam> workflow_params_;

    tf::Executor executor_; // 共享 Taskflow 线程池
};
