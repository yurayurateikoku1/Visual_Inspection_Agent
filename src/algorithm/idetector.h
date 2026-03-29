#pragma once
#include "app/common.h"

/// @brief 产品检测器抽象接口
///        每种产品对应一个实现，内部自由组合算法，由 WorkflowManager 调用
///        参数通过各子类构造函数传入，不经过基类接口
class IDetector
{
public:
    virtual ~IDetector() = default;

    /// @brief 执行检测，结果写入 ctx.result 和 ctx.display_image
    virtual void detect(NodeContext &ctx) = 0;
};
