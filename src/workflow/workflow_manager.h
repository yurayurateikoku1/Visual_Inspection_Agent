#pragma once

#include "algorithm/idetector.h"
#include "app/common.h"
#include <taskflow/taskflow.hpp>
#include <QObject>
#include <halconcpp/HalconCpp.h>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>

/// @brief 工作流管理器（单例）
///
/// 职责：管理所有检测工作流的调度与执行。
/// 每�� WorkflowParam（key = camera_name + "_" + di_addr）对应一个独立的 PipelineState 状态机。
/// 4 个相机 × 4 个 DI 触发 = 最多 16 条并行工作流。
///
/// @par 状态机（事件驱动，集中在 dispatch() 处理）
///
///  State\\Event      DI_RISING            FRAME_ARRIVED      INSPECT_DONE        HOLD_EXPIRED
///  ──────────────────────────────────────────────────────────────────────────────────────────────
///  IDLE              → WAITING/INSPECTING  -                  -                   -
///  WAITING_FRAME     -                     → INSPECTING       -                   -
///  INSPECTING        -                     -                  → HOLDING_RESULT    -
///  HOLDING_RESULT    -                     -                  -                   → IDLE
///
/// @par 三个事件入口
/// - slot_onIOStateUpdated()  — CommManager 200ms IO 轮询 emit 触发（主线程）
///   - IDLE: 检测 DI 上升沿 → dispatch(DI_RISING)
///   - HOLDING_RESULT: DI 恢复 + hold 时间到 → dispatch(HOLD_EXPIRED)
/// - onFrameArrived()         — CameraViewWidget 帧到达（主线程）
///   - WAITING_FRAME → dispatch(FRAME_ARRIVED)
/// - runInspection() 完成后   — Taskflow 线程 invokeMethod 回主线程
///   - INSPECTING → dispatch(INSPECT_DONE)
///
/// @par 线程归属
/// - dispatch() / slot_onIOStateUpdated() / onFrameArrived()：Qt 主线程（微秒级，不阻塞）
/// - runInspection()：Taskflow 线程池（纯计算：capture → ROI → detect → timestamp，跑完立即释放）
/// - writeDO()：通过 invokeMethod 投递到 CommManager 工作线程执行
class WorkflowManager : public QObject
{
    Q_OBJECT
public:
    static WorkflowManager &getInstance();

    /// @brief 根据 AppContext::workflow_params 构建所有 PipelineState 和检测器
    void buildAll();

    /// @brief 开始自动检测（清除 DO，重置状态，开始响应 DI 触发）
    void startAll();

    /// @brief 停止自动检测（等待 Taskflow 任务完成，清除 DO）
    void stopAll();

    bool isRunning() const { return running_; }

    /// @brief 手动触发一次检测（离线测试用），跳过 DI，直接用当前帧缓存
    void triggerOnce(const std::string &workflow_key);

    /// @brief 参数变更后重建指定 workflow 的检测器（仅 IDLE 状态可调用）
    void rebuildWorkflow(const std::string &workflow_key);

signals:
    /// @brief 采集完成，用于 UI 显示原始帧
    void sign_frameCaptured(const std::string &camera_name, const HalconCpp::HObject &image);

    /// @brief 检测完成，携带结果和显示图像
    void sign_inspectionFinished(const std::string &workflow_key, const std::string &camera_name,
                                 const HalconCpp::HObject &display_image, const InspectionResult &result);

    /// @brief 运行状态变化
    void sign_runningChanged(bool running);

    /// @brief rebuildWorkflow 因非 IDLE 状态而失败
    void sign_rebuildFailed(const std::string &workflow_key);

public slots:
    /// @brief 接收 CameraViewWidget::frameArrived，更新帧缓存，WAITING_FRAME 时触发检测
    void onFrameArrived(const std::string &camera_name, const HalconCpp::HObject &frame);

private slots:
    /// @brief CommManager 200ms IO 轮询触发，处理 DI 上升沿和 HOLDING_RESULT 超时
    void slot_onIOStateUpdated();

private:
    WorkflowManager();
    ~WorkflowManager();

    // ── 状态机 ──

    enum class State {
        IDLE,            ///< 空闲，等待触发
        WAITING_FRAME,   ///< DI 已触发，等待相机帧到达
        INSPECTING,      ///< 正在检测（Taskflow 线程执行中）
        HOLDING_RESULT   ///< 检测完成，DO 已输出，等待 DI 恢复后清除
    };

    enum class Event {
        DI_RISING,       ///< DI 上升沿（PLC 触发信号 0→1）
        FRAME_ARRIVED,   ///< 相机帧到达
        INSPECT_DONE,    ///< 检测执行完毕
        HOLD_EXPIRED     ///< 保持时间到期且 DI 已恢复，可以清 DO 回空闲
    };

    /// @brief 单条工作流的运行时状态
    struct PipelineState
    {
        WorkflowParam              param;          ///< 工作流配置（相机/通信/触发/IO/检测器参数）
        std::unique_ptr<IDetector> detector;        ///< 检测器实例（模型加载一次，复用）
        std::atomic<State>         state{State::IDLE};
        bool                       last_di_state = false; ///< 上一次 DI 电平，用于边沿检测

        HalconCpp::HObject         latest_frame;   ///< 最新帧缓存（onFrameArrived 覆盖写）
        HalconCpp::HObject         offline_image;   ///< 离线测试图片
        std::mutex                 frame_mutex;     ///< 保护 latest_frame / offline_image

        bool                       last_result_pass = true;  ///< 上次检测结果，用于 HOLDING_RESULT 阶段写 DO
        std::chrono::steady_clock::time_point hold_start;    ///< 进入 HOLDING_RESULT 的时刻
    };

    /// @brief 集中状态转移（仅主线程调用），所有转移逻辑在此 switch-case
    void dispatch(const std::string &key, PipelineState &ps, Event event);

    /// @brief 检测执行（Taskflow 线程）：delay → capture → ROI → detect → timestamp
    ///        完成后 invokeMethod 回主线程 dispatch(INSPECT_DONE)
    void runInspection(const std::string &key);

    /// @brief 根据 WorkflowParam::detector_param variant 创建对应 IDetector
    static std::unique_ptr<IDetector> createDetector(const WorkflowParam &param);

    /// @brief 写 DO 输出（通过 invokeMethod 投递到 CommManager 工作线程）
    void writeDO(const PipelineState &ps, bool ok_val, bool ng_val);

    std::map<std::string, std::unique_ptr<PipelineState>> pipelines_; ///< key → 状态机
    std::atomic<bool> running_{false};
    tf::Executor &executor_;   ///< 共享 Taskflow 线程池（来自 AppContext）
};
