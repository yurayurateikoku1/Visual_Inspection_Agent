#pragma once

#include "inspection_pipeline.h"
#include "app/common.h"
#include <taskflow/taskflow.hpp>
#include <QObject>
#include <map>
#include <array>
#include <memory>
#include <atomic>
#include <mutex>

/// @brief 工作流管理器
///        每条 WorkflowParam 对应一个独立状态机：
///        IDLE → WAITING_FRAME（DI上升沿）→ INSPECTING（帧到达）→ HOLDING_RESULT → IDLE
class WorkflowManager : public QObject
{
    Q_OBJECT
public:
    static WorkflowManager &getInstance();

    /// @brief 根据 AppContext 中的 WorkflowParam 构建所有 Pipeline
    void buildAll();

    /// @brief 开始自动检测（响应 DI 触发）
    void startAll();

    /// @brief 停止自动检测
    void stopAll();

    bool isRunning() const { return running_; }

    /// @brief 手动触发（离线测试用）：跳过 DI 检测，直接用当前最新帧
    void triggerOnce(const std::string &workflow_name);

    /// @brief 参数变更后重建指定 workflow 的 Pipeline
    void rebuildWorkflow(const std::string &workflow_name);

signals:
    void sign_frameCaptured(const std::string &camera_name, const HalconCpp::HObject &image);
    void sign_inspectionFinished(const std::string &workflow_name, const std::string &camera_name,
                                 const HalconCpp::HObject &display_image, const InspectionResult &result);
    void sign_runningChanged(bool running);
    /// @brief rebuildWorkflow 因 workflow 非 IDLE 状态而失败时发出
    void sign_rebuildFailed(const std::string &workflow_name);

public slots:
    /// @brief 接收 CameraViewWidget::frameArrived，更新各 workflow 的最新帧缓存
    ///        若该 workflow 处于 WAITING_FRAME 状态则立即触发检测
    void onFrameArrived(const std::string &camera_name, const HalconCpp::HObject &frame);

private slots:
    void slot_onIOStateUpdated();

private:
    WorkflowManager();
    ~WorkflowManager();

    void executeWorkflow(const std::string &workflow_name);

    enum class State { IDLE, WAITING_FRAME, INSPECTING, HOLDING_RESULT };

    struct PipelineState
    {
        WorkflowParam param;
        std::unique_ptr<InspectionPipeline> pipeline;
        std::atomic<State> state{State::IDLE};
        bool last_di_state = false;

        HalconCpp::HObject latest_frame; // 最新帧缓存（onFrameArrived 覆盖写）
        std::mutex frame_mutex;
    };

    std::map<std::string, std::unique_ptr<PipelineState>> pipelines_;
    std::array<bool, 8> last_di_state_{};
    std::atomic<bool> running_{false};
    tf::Executor &executor_;
};
