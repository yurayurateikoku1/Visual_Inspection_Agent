#pragma once

#include <taskflow/taskflow.hpp>

/// @brief 全局运行时上下文：持有各模块共享的运行时资源
class AppContext
{
public:
    static AppContext &instance();

    /// @brief 获取全局 Taskflow Executor（检测流水线共享线程池）
    tf::Executor &executor() { return executor_; }

private:
    AppContext() = default;

    tf::Executor executor_; // 默认线程数 = CPU 核心数
};
