#pragma once

#include "inspection_pipeline.h"
#include "app/common.h"
#include <QObject>
#include <map>
#include <array>
#include <memory>
#include <atomic>

/// @brief 工作流管理器
///        监听 CommManager 的 DI 状态变化，检测上升沿后触发对应 Pipeline
///        状态机：DI触发 → 延时 → 软触发拍照 → 算法检测 → DO输出
class WorkflowManager : public QObject
{
    Q_OBJECT
public:
    static WorkflowManager &getInstance();

    /// @brief 根据配置构建所有 Pipeline
    void buildAll();

    /// @brief 开始自动检测
    void startAll();

    /// @brief 停止自动检测
    void stopAll();

    bool isRunning() const { return running_; }

    /// @brief 手动触发指定工作流
    void triggerOnce(const std::string &workflow_id);

signals:
    /// @brief 采集完成，发送原图到 UI 显示
    void sign_frameCaptured(const std::string &camera_id, const HalconCpp::HObject &image);

    /// @brief 检测完成，发送叠加结果的显示图 + 检测结果
    void sign_inspectionFinished(const std::string &workflow_id, const std::string &camera_id,
                                 const HalconCpp::HObject &display_image, const InspectionResult &result);

    /// @brief 运行状态变化
    void sign_runningChanged(bool running);

private slots:
    /// @brief 响应 CommManager IO 状态更新，检测 DI 上升沿
    void slot_onIOStateUpdated();

private:
    WorkflowManager();
    ~WorkflowManager();

    /// @brief 执行一次检测流程
    void executeWorkflow(const std::string &workflow_id);

    struct PipelineState
    {
        WorkflowParam param;
        std::unique_ptr<InspectionPipeline> pipeline;
        std::atomic<bool> busy{false}; // 正在执行中，防止重入
        bool last_di_state = false;    // 上一次 DI 状态，用于检测上升沿
    };

    std::map<std::string, std::unique_ptr<PipelineState>> pipelines_;
    std::array<bool, 8> last_di_state_{};
    std::atomic<bool> running_{false};
    tf::Executor &executor_; // 引用 AppContext 持有的共享线程池
};
