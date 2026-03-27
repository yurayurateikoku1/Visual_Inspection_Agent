#pragma once

#include "common.h"
#include <taskflow/taskflow.hpp>
#include <map>
#include <vector>
#include <algorithm>

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

    /// @brief 按相机名查找其所有工作流名称（最多4条，按 trigger_di_addr 排序）
    std::vector<std::string> workflowNamesForCamera(const std::string &camera_name) const
    {
        std::vector<std::pair<uint16_t, std::string>> tmp;
        for (auto &[name, wf] : workflow_params_)
            if (wf.camera_name == camera_name)
                tmp.emplace_back(wf.trigger_di_addr, name);
        std::sort(tmp.begin(), tmp.end());
        std::vector<std::string> result;
        for (auto &[_, name] : tmp)
            result.push_back(name);
        return result;
    }

    /// @brief 确保每个相机都有4条 WorkflowParam（启动时初始化用）
    void ensureWorkflowsForAllCameras()
    {
        for (auto &[cam_name, _] : camera_params_)
        {
            for (int di = 0; di < 4; ++di)
            {
                std::string wf_name = "wf_" + cam_name + "_" + std::to_string(di);
                if (workflow_params_.count(wf_name) == 0)
                {
                    WorkflowParam wp;
                    wp.name = wf_name;
                    wp.camera_name = cam_name;
                    wp.trigger_di_addr = static_cast<uint16_t>(di);
                    wp.enabled = false;
                    workflow_params_[wf_name] = wp;
                }
            }
        }
    }

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
