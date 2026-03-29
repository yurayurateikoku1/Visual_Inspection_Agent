#include "workflow_manager.h"
#include "algorithm/terminal_detector.h"
#include "app/app_context.h"
#include "camera/camera_manager.h"
#include "communication/comm_manager.h"
#include <spdlog/spdlog.h>
#include <QMetaObject>
#include <thread>

// ═══════════════════════════════════════════════════════════════════
// 单例 & 生命周期
// ═══════════════════════════════════════════════════════════════════

WorkflowManager &WorkflowManager::getInstance()
{
    static WorkflowManager inst;
    return inst;
}

/// 构造时订阅 CommManager 的 IO 轮询信号。
/// CommManager 在独立工作线程，WorkflowManager 在主线程，
/// 跨线程信号自动走 QueuedConnection，所以 slot 在主线程执行。
WorkflowManager::WorkflowManager()
    : executor_(AppContext::getInstance().executor())
{
    connect(&CommManager::getInstance(), &CommManager::sign_ioStateUpdated,
            this, &WorkflowManager::slot_onIOStateUpdated);
}

WorkflowManager::~WorkflowManager()
{
    stopAll();
}

// ═══════════════════════════════════════════════════════════════════
// 检测器工厂
// ═══════════════════════════════════════════════════════════════════

/// 根据 WorkflowParam::detector_param（variant）的实际类型创建对应的 IDetector。
/// monostate → nullptr（不检测），TerminalParam → TerminalDetector。
/// 新增产品只需加一个 else if constexpr 分支。
std::unique_ptr<IDetector> WorkflowManager::createDetector(const WorkflowParam &param)
{
    std::unique_ptr<IDetector> det;
    std::visit([&](const auto &p)
    {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, TerminalParam>)
            det = std::make_unique<TerminalDetector>(p);
        // 新产品：else if constexpr (std::is_same_v<T, XxxParam>) { det = std::make_unique<XxxDetector>(p); }
    }, param.detector_param);
    return det;
}

// ═══════════════════════════════════════════════════════════════════
// buildAll / startAll / stopAll
//
// 调用时机：
//   buildAll()  — 程序启动时，ConfigManager 加载配置后调用
//   startAll()  — 用户点击「开始检测」
//   stopAll()   — 用户点击「停止检测」或程序退出
// 均在主线程调用。
// ═══════════════════════════════════════════════════════════════════

/// 从 AppContext::workflow_params 读取配置，为每条 workflow 创建 PipelineState。
/// enabled 的 workflow 会立即加载检测器模型（可能耗时几百 ms）。
void WorkflowManager::buildAll()
{
    pipelines_.clear();

    for (auto &[key, wp] : AppContext::getInstance().workflow_params)
    {
        auto ps = std::make_unique<PipelineState>();
        ps->param = wp;
        ps->state = State::IDLE;
        ps->last_di_state = false;

        if (wp.enabled)
            ps->detector = createDetector(wp);

        spdlog::info("Workflow {} registered: camera={} DI={} enabled={}",
                     key, wp.camera_name, wp.trigger.di_addr, wp.enabled);

        pipelines_[key] = std::move(ps);
    }
}

/// 清除所有 DO 输出，重置状态机为 IDLE，开始响应 DI 触发。
void WorkflowManager::startAll()
{
    if (running_)
        return;

    for (auto &[key, ps] : pipelines_)
    {
        writeDO(*ps, false, false);
        ps->state = State::IDLE;
        ps->last_di_state = false;
    }

    running_ = true;
    emit sign_runningChanged(true);
    spdlog::info("WorkflowManager started, {} pipelines", pipelines_.size());
}

/// 标记停止 → 等待 Taskflow 线程池中所有任务完成 → 清 DO → 重置状态。
/// wait_for_all() 会阻塞主线程直到 runInspection 全部返回，保证不会有野线程残留。
void WorkflowManager::stopAll()
{
    if (!running_)
        return;

    running_ = false;
    executor_.wait_for_all();

    for (auto &[key, ps] : pipelines_)
    {
        writeDO(*ps, false, false);
        ps->state = State::IDLE;
    }

    emit sign_runningChanged(false);
    spdlog::info("WorkflowManager stopped");
}

// ═══════════════════════════════════════════════════════════════════
// rebuildWorkflow / triggerOnce
// ═══════════════════════════════════════════════════════════════════

/// 用户在 WorkflowConfigDialog 修改参数后调用。
/// 仅 IDLE 状态才能 rebuild（检测中途不能换模型），失败 emit sign_rebuildFailed。
void WorkflowManager::rebuildWorkflow(const std::string &workflow_key)
{
    auto it = pipelines_.find(workflow_key);
    if (it == pipelines_.end())
        return;

    auto *ps = it->second.get();
    if (ps->state != State::IDLE)
    {
        spdlog::warn("Workflow {} is not idle, cannot rebuild", workflow_key);
        emit sign_rebuildFailed(workflow_key);
        return;
    }

    // 从 AppContext 重新读取最新参数
    auto &wf_map = AppContext::getInstance().workflow_params;
    auto wf_it = wf_map.find(workflow_key);
    if (wf_it == wf_map.end())
        return;

    ps->param = wf_it->second;
    ps->detector = ps->param.enabled ? createDetector(ps->param) : nullptr;

    spdlog::info("Workflow {} rebuilt", workflow_key);
}

/// 手动触发一次检测（离线测试 / 调试用）。
/// 直接 dispatch DI_RISING 事件，跳过 DI 信号，使用当前帧缓存或离线图。
void WorkflowManager::triggerOnce(const std::string &workflow_key)
{
    auto it = pipelines_.find(workflow_key);
    if (it == pipelines_.end())
        return;

    auto *ps = it->second.get();
    dispatch(workflow_key, *ps, Event::DI_RISING);
}

// ═══════════════════════════════════════════════════════════════════
// 事件入口 ①：帧到达
//
// 调用方：CameraViewWidget::frameArrived 信号 → MainWindow 转发
// 线程：主线程
// ═══════════════════════════════════════════════════════════════════

/// 遍历所有 workflow，找到绑定该相机的条目，更新帧缓存。
/// 如果正好在 WAITING_FRAME 状态（DI 已触发但帧还没来），立即触发检测。
void WorkflowManager::onFrameArrived(const std::string &camera_name,
                                     const HalconCpp::HObject &frame)
{
    for (auto &[key, ps] : pipelines_)
    {
        if (ps->param.camera_name != camera_name)
            continue;

        std::lock_guard lk(ps->frame_mutex);
        ps->latest_frame = frame;

        if (ps->state == State::WAITING_FRAME)
            dispatch(key, *ps, Event::FRAME_ARRIVED);
    }
}

// ═══════════════════════════════════════════════════════════════════
// 事件入口 ②：IO 状态更新
//
// 调用方：CommManager 工作线程 200ms 定时器 → emit sign_ioStateUpdated
//         → QueuedConnection → 本槽在主线程执行
// 职责：
//   1. IDLE 状态：检测 DI 上升沿 → dispatch(DI_RISING)
//   2. HOLDING_RESULT 状态：检查 DI 是否恢复 + hold 时间 → dispatch(HOLD_EXPIRED)
// ═══════════════════════════════════════════════════════════════════

void WorkflowManager::slot_onIOStateUpdated()
{
    if (!running_)
        return;

    // 从 CommManager 拷贝一份 DI 状态（加锁拷贝 8 个 bool，微秒级）
    auto di = CommManager::getInstance().diState();
    auto now = std::chrono::steady_clock::now();

    for (auto &[key, ps] : pipelines_)
    {
        if (!ps->param.enabled)
            continue;

        uint16_t addr = ps->param.trigger.di_addr;
        if (addr >= 8)
            continue;

        // 边沿检测：上一次为 false，这一次为 true → 上升沿
        bool current_di = di[addr];
        bool rising_edge = current_di && !ps->last_di_state;
        ps->last_di_state = current_di;

        switch (ps->state)
        {
        case State::IDLE:
            if (rising_edge)
                dispatch(key, *ps, Event::DI_RISING);
            break;

        case State::WAITING_FRAME:
            break;  // 等帧，由 onFrameArrived 驱动

        case State::INSPECTING:
            break;  // Taskflow 线程在跑，不干预

        case State::HOLDING_RESULT:
        {
            // 两个条件都满足才清 DO：① DI 恢复低电平 ② 保持时间 ≥ result_hold_ms
            // 超时 5s 强制清除，防止 PLC 异常导致 DO 永久保持
            bool di_cleared = !current_di;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - ps->hold_start).count();

            if ((di_cleared && elapsed_ms >= ps->param.io.result_hold_ms) || elapsed_ms > 5000)
            {
                if (elapsed_ms > 5000)
                    spdlog::warn("Workflow {} DI clear timeout", key);
                dispatch(key, *ps, Event::HOLD_EXPIRED);
            }
            break;
        }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// 集中状态转移（仅主线程调用）
//
// 所有状态转移逻辑集中在此 switch-case，不散落在各处。
// 非法的 state+event 组合直接忽略（保持原状态）。
//
//  State\Event      DI_RISING            FRAME_ARRIVED      INSPECT_DONE       HOLD_EXPIRED
//  ─────────────────────────────────────────────────────────────────────────────────────────
//  IDLE             → WAITING/INSPECTING  -                  -                  -
//  WAITING_FRAME    -                     → INSPECTING       -                  -
//  INSPECTING       -                     -                  → HOLDING_RESULT   -
//  HOLDING_RESULT   -                     -                  -                  → IDLE
// ═══════════════════════════════════════════════════════════════════

void WorkflowManager::dispatch(const std::string &key, PipelineState &ps, Event event)
{
    switch (ps.state)
    {
    case State::IDLE:
        if (event == Event::DI_RISING)
        {
            spdlog::info("DI{} rising → workflow {}", ps.param.trigger.di_addr, key);

            // 有帧（或离线图）→ 立即进入 INSPECTING
            // 无帧 → 等相机回图，进入 WAITING_FRAME
            std::lock_guard lk(ps.frame_mutex);
            if (ps.latest_frame.IsInitialized() || ps.offline_image.IsInitialized())
            {
                ps.state = State::INSPECTING;
                executor_.silent_async([this, key]() { runInspection(key); });
            }
            else
            {
                ps.state = State::WAITING_FRAME;
            }
        }
        break;

    case State::WAITING_FRAME:
        if (event == Event::FRAME_ARRIVED)
        {
            // 帧来了，提交到 Taskflow 线程池执行检测
            ps.state = State::INSPECTING;
            executor_.silent_async([this, key]() { runInspection(key); });
        }
        break;

    case State::INSPECTING:
        if (event == Event::INSPECT_DONE)
        {
            // 检测完成 → 写 DO(OK 或 NG) → 进入保持阶段
            bool pass = ps.last_result_pass;
            if (!ps.param.comm_name.empty())
                writeDO(ps, pass, !pass);

            ps.hold_start = std::chrono::steady_clock::now();
            ps.state = State::HOLDING_RESULT;
        }
        break;

    case State::HOLDING_RESULT:
        if (event == Event::HOLD_EXPIRED)
        {
            // DI 恢复 + 保持时间到 → 清除 DO → 回到空闲
            writeDO(ps, false, false);
            ps.state = State::IDLE;
        }
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════
// 检测执行（Taskflow 线程池，纯计算）
//
// 执行流程：delay → capture → ROI裁剪 → detect → timestamp
// 跑完立即释放线程，不做任何等待。
// 状态推进通过 invokeMethod 回主线程 dispatch(INSPECT_DONE)。
//
// 注意：本函数在 Taskflow 工作线程执行，只能访问：
//   - ps->param（只读，主线程不会在 INSPECTING 期间修改）
//   - ps->detector（只读指针，检测器内部线程安全）
//   - ps->offline_image / latest_frame（加 frame_mutex 保护）
//   - ps->last_result_pass（写一次，主线程在 INSPECT_DONE 后才读）
//   - ctx（局部变量，不共享）
// ═══════════════════════════════════════════════════════════════════

void WorkflowManager::runInspection(const std::string &key)
{
    auto it = pipelines_.find(key);
    if (it == pipelines_.end())
        return;

    auto *ps = it->second.get();
    auto &param = ps->param;
    bool offline = ps->offline_image.IsInitialized();

    spdlog::info("Workflow {} executing{}...", key, offline ? " (offline)" : "");

    // ── 触发延时 + 曝光覆盖（仅在线模式） ──
    if (!offline)
    {
        if (param.trigger.delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(param.trigger.delay_ms));

        if (param.camera_override.exposure_time > 0)
        {
            auto *cam = CameraManager::getInstance().getCamera(param.camera_name);
            if (cam)
                cam->setExposureTime(param.camera_override.exposure_time);
        }
    }

    // ── 采集图像 ──
    NodeContext ctx;
    ctx.camera_name = param.camera_name;
    ctx.result.pass = true;

    if (offline)
    {
        ctx.image = ps->offline_image;
        ctx.display_image = ps->offline_image;
    }
    else
    {
        auto *cam = CameraManager::getInstance().getCamera(param.camera_name);
        if (!cam || !cam->grabOne(ctx.image))
        {
            spdlog::error("Workflow {}: capture failed", key);
            ps->state = State::IDLE;  // 采集失败直接回 IDLE
            return;
        }
        ctx.display_image = ctx.image;
    }

    // 通知 UI 显示原始帧
    emit sign_frameCaptured(param.camera_name, ctx.image);

    // ── ROI 裁剪 ──
    if (param.roi.enabled)
    {
        try
        {
            HalconCpp::HObject cropped;
            HalconCpp::CropRectangle1(ctx.image, &cropped,
                                      static_cast<int>(param.roi.row1), static_cast<int>(param.roi.col1),
                                      static_cast<int>(param.roi.row2), static_cast<int>(param.roi.col2));
            ctx.image = cropped;
            ctx.display_image = cropped;
        }
        catch (const HalconCpp::HException &e)
        {
            spdlog::error("Workflow {}: ROI crop failed: {}", key, e.ErrorMessage().Text());
            ctx.result.pass = false;
        }
    }

    // ── 检测（调用 IDetector，如 TerminalDetector 跑 YOLO 推理） ──
    if (ps->detector)
        ps->detector->detect(ctx);

    // ── 打时间戳 ──
    auto now = std::chrono::system_clock::now();
    ctx.result.timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    spdlog::info("Workflow {}: pass={} defects={}", key, ctx.result.pass, ctx.result.defects.size());

    // ── 清离线图 ──
    {
        std::lock_guard lk(ps->frame_mutex);
        ps->offline_image.Clear();
    }

    // 缓存结果供 dispatch(INSPECT_DONE) 使用，通知 UI 显示结果
    ps->last_result_pass = ctx.result.pass;
    emit sign_inspectionFinished(key, param.camera_name, ctx.display_image, ctx.result);

    // 回到主线程推进状态机：INSPECTING → HOLDING_RESULT
    QMetaObject::invokeMethod(this, [this, key]()
    {
        auto it2 = pipelines_.find(key);
        if (it2 != pipelines_.end())
            dispatch(key, *it2->second, Event::INSPECT_DONE);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════
// DO 输出辅助
//
// 通过 invokeMethod 投递到 CommManager 工作线程执行 Modbus 写线圈。
// 调用方可以在任意线程（主线程的 dispatch、Taskflow 的 runInspection 都会调用）。
// ═══════════════════════════════════════════════════════════════════

void WorkflowManager::writeDO(const PipelineState &ps, bool ok_val, bool ng_val)
{
    if (ps.param.comm_name.empty())
        return;

    auto &comm = CommManager::getInstance();
    QMetaObject::invokeMethod(&comm,
        [&comm, name = ps.param.comm_name,
         ok_addr = ps.param.io.do_ok_addr, ng_addr = ps.param.io.do_ng_addr,
         ok_val, ng_val]()
        {
            comm.writeSingleCoil(name, ok_addr, ok_val);
            comm.writeSingleCoil(name, ng_addr, ng_val);
        }, Qt::QueuedConnection);
}
