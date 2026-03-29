#pragma once

#include "common.h"
#include <taskflow/taskflow.hpp>
#include <map>
#include <vector>
#include <algorithm>

/// @brief 全局运行时上下文：持有运行时参数和共享资源
///        ConfigManager 负责初始化加载和关闭时回写，AppContext 负责运行时持有
struct AppContext
{
    static AppContext &getInstance();

    // 运行时参数，以 name 为键
    std::map<std::string, CameraParam>        camera_params;
    std::map<std::string, CommunicationParam> comm_params;
    LightParam                                light_param;
    std::map<std::string, WorkflowParam>      workflow_params;

    // UI 交互状态（供多个面板共享）
    std::string                               selected_camera_name;
    std::map<std::string, std::string>        selected_workflows; ///< camera_name → workflow_name

    // 共享任务池（不可拷贝，通过方法访问）
    tf::Executor &executor() { return executor_; }

    /// @brief 按相机名查找其所有 workflow key（最多4条，按 trigger_di_addr 排序）
    std::vector<std::string> workflowKeysForCamera(const std::string &camera_name) const
    {
        std::vector<std::pair<uint16_t, std::string>> tmp;
        for (auto &[key, wf] : workflow_params)
            if (wf.camera_name == camera_name)
                tmp.emplace_back(wf.trigger.di_addr, key);
        std::sort(tmp.begin(), tmp.end());
        std::vector<std::string> result;
        for (auto &[_, key] : tmp)
            result.push_back(key);
        return result;
    }

    /// @brief 确保每个相机都有4条 WorkflowParam（启动时初始化用）
    void ensureWorkflowsForAllCameras()
    {
        for (auto &[cam_name, _] : camera_params)
        {
            for (int di = 0; di < 4; ++di)
            {
                WorkflowParam wp;
                wp.camera_name     = cam_name;
                wp.trigger.di_addr = static_cast<uint16_t>(di);
                wp.enabled         = false;
                if (workflow_params.count(wp.key()) == 0)
                    workflow_params[wp.key()] = wp;
            }
        }
    }

private:
    AppContext() = default;
    tf::Executor executor_;
};
