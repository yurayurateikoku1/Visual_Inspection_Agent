#pragma once

#include <string>
#include <memory>
#include "app/common.h"
#include <nlohmann/json.hpp>

class QWidget;
class CameraViewWidget;

/// 算法分类常量
namespace AlgorithmCategory
{
    constexpr const char *TOOL = "工具";
    constexpr const char *AI_INFER = "AI推理";
}

/// 算法接口
class IAlgorithm
{
public:
    virtual ~IAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual std::string category() const = 0;

    /// 配置阶段：点击流程列表中的该步骤时调用
    /// view 用于画 ROI 等交互操作，parent 用于弹 Dialog
    /// params 读写算法参数（持久化到 config）
    virtual void configure(CameraViewWidget *view, QWidget *parent,
                           nlohmann::json &params) = 0;

    /// 从已保存的参数恢复状态（pipeline build 时调用）
    virtual void loadParams(const nlohmann::json &params) = 0;

    /// 运行阶段：处理图像，结果写入 ctx
    virtual bool process(NodeContext &ctx) = 0;
};

/// 根据 ID 创建算法实例
std::unique_ptr<IAlgorithm> createAlgorithm(const std::string &id);
