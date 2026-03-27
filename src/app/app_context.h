#pragma once

#include "common.h"
#include <taskflow/taskflow.hpp>
#include <map>

/// @brief 全局运行时上下文：持有运行时参数和共享资源
///        ConfigManager 负责初始化加载和关闭时回写，AppContext 负责运行时持有
///        所有参数以 name 为键存储在 std::map 中，方便按名称直接查找
class AppContext
{
public:
    static AppContext &getInstance();

    // 运行时参数（各模块读写），以 name 为键的哈希表

    std::map<std::string, CameraParam> &cameraParams() { return camera_params_; }
    std::map<std::string, CommunicationParam> &commParams() { return comm_params_; }
    LightParam &lightParam() { return light_param_; }
    std::map<std::string, WorkflowParam> &workflowParams() { return workflow_params_; }

    const std::map<std::string, CameraParam> &cameraParams() const { return camera_params_; }
    const std::map<std::string, CommunicationParam> &commParams() const { return comm_params_; }
    const LightParam &lightParam() const { return light_param_; }
    const std::map<std::string, WorkflowParam> &workflowParams() const { return workflow_params_; }

    // 共享任务池
    tf::Executor &executor() { return executor_; }

private:
    AppContext();
    ~AppContext() = default;

    std::map<std::string, CameraParam> camera_params_;
    std::map<std::string, CommunicationParam> comm_params_;
    LightParam light_param_;
    std::map<std::string, WorkflowParam> workflow_params_;

    tf::Executor executor_; // 共享 Taskflow 线程池
};
