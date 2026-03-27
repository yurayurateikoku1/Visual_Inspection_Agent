#include "workflow_manager.h"
#include "app/app_context.h"
#include "camera/camera_manager.h"
#include "communication/comm_manager.h"
#include <spdlog/spdlog.h>
#include <QMetaObject>
#include <chrono>
#include <thread>

WorkflowManager &WorkflowManager::getInstance()
{
    static WorkflowManager inst;
    return inst;
}

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

void WorkflowManager::buildAll()
{
    pipelines_.clear();

    for (auto &[wf_name, wp] : AppContext::getInstance().workflowParams())
    {
        auto state          = std::make_unique<PipelineState>();
        state->param        = wp;
        state->pipeline     = std::make_unique<InspectionPipeline>(wp);
        state->last_di_state = false;
        state->state        = State::IDLE;

        if (wp.enabled)
            state->pipeline->build();

        SPDLOG_INFO("Workflow {} registered: camera={} DI={} enabled={}",
                    wp.name, wp.camera_name, wp.trigger_di_addr, wp.enabled);

        pipelines_[wf_name] = std::move(state);
    }
}

void WorkflowManager::startAll()
{
    if (running_)
        return;

    auto &comm = CommManager::getInstance();
    for (auto &[name, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm,
            [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr,
             comm_name = state->param.comm_name]()
            {
                comm.writeSingleCoil(comm_name, ok, false);
                comm.writeSingleCoil(comm_name, ng, false);
            }, Qt::QueuedConnection);

        state->state         = State::IDLE;
        state->last_di_state = false;
    }

    running_ = true;
    emit sign_runningChanged(true);
    SPDLOG_INFO("WorkflowManager started, {} pipelines", pipelines_.size());
}

void WorkflowManager::stopAll()
{
    if (!running_)
        return;

    running_ = false;
    executor_.wait_for_all();

    auto &comm = CommManager::getInstance();
    for (auto &[name, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm,
            [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr,
             comm_name = state->param.comm_name]()
            {
                comm.writeSingleCoil(comm_name, ok, false);
                comm.writeSingleCoil(comm_name, ng, false);
            }, Qt::QueuedConnection);

        state->state = State::IDLE;
    }

    emit sign_runningChanged(false);
    SPDLOG_INFO("WorkflowManager stopped");
}

void WorkflowManager::rebuildWorkflow(const std::string &workflow_name)
{
    auto it = pipelines_.find(workflow_name);
    if (it == pipelines_.end())
        return;

    auto *state = it->second.get();
    if (state->state != State::IDLE)
    {
        SPDLOG_WARN("Workflow {} is not idle, cannot rebuild now", workflow_name);
        emit sign_rebuildFailed(workflow_name);
        return;
    }

    auto &wf_map = AppContext::getInstance().workflowParams();
    auto wf_it   = wf_map.find(workflow_name);
    if (wf_it == wf_map.end())
        return;

    state->param    = wf_it->second;
    state->pipeline = std::make_unique<InspectionPipeline>(state->param);
    if (state->param.enabled)
        state->pipeline->build();

    SPDLOG_INFO("Workflow {} rebuilt", workflow_name);
}

void WorkflowManager::triggerOnce(const std::string &workflow_name)
{
    auto it = pipelines_.find(workflow_name);
    if (it == pipelines_.end())
    {
        SPDLOG_WARN("Workflow {} not found", workflow_name);
        return;
    }

    auto *state = it->second.get();
    if (state->state != State::IDLE)
    {
        SPDLOG_WARN("Workflow {} not idle, skipping trigger", workflow_name);
        return;
    }

    // 加锁：注入帧并切换状态，防止与 Taskflow 线程竞争
    {
        std::lock_guard lk(state->frame_mutex);
        if (state->latest_frame.IsInitialized())
            state->pipeline->setOfflineImage(state->latest_frame);
        state->state = State::INSPECTING;
    }

    executor_.silent_async([this, workflow_name]()
                           { executeWorkflow(workflow_name); });
}

void WorkflowManager::onFrameArrived(const std::string &camera_name,
                                      const HalconCpp::HObject &frame)
{
    for (auto &[wf_name, state] : pipelines_)
    {
        if (state->param.camera_name != camera_name)
            continue;

        // 加锁：更新帧缓存 + 条件触发检测，两步必须原子
        {
            std::lock_guard lk(state->frame_mutex);
            state->latest_frame = frame;

            if (state->state == State::WAITING_FRAME)
            {
                state->pipeline->setOfflineImage(state->latest_frame);
                state->state = State::INSPECTING;
                executor_.silent_async([this, wf_name]()
                                       { executeWorkflow(wf_name); });
            }
        }
    }
}

void WorkflowManager::slot_onIOStateUpdated()
{
    if (!running_)
        return;

    auto di = CommManager::getInstance().diState();

    for (auto &[wf_name, state] : pipelines_)
    {
        if (!state->param.enabled)
            continue;

        uint16_t addr = state->param.trigger_di_addr;
        if (addr >= 8)
            continue;

        bool current_di = di[addr];
        bool rising_edge = current_di && !state->last_di_state;
        state->last_di_state = current_di;

        if (!rising_edge)
            continue;

        // DI 上升沿：IDLE → WAITING_FRAME 或直接 INSPECTING（若已有帧缓存）
        {
            std::lock_guard lk(state->frame_mutex);
            if (state->state != State::IDLE)
                continue;

            SPDLOG_INFO("DI{} rising edge → workflow {}", addr, wf_name);

            if (state->latest_frame.IsInitialized())
            {
                state->pipeline->setOfflineImage(state->latest_frame);
                state->state = State::INSPECTING;
                executor_.silent_async([this, wf_name]()
                                       { executeWorkflow(wf_name); });
            }
            else
            {
                state->state = State::WAITING_FRAME;
            }
        }
    }
}

void WorkflowManager::executeWorkflow(const std::string &workflow_name)
{
    auto it = pipelines_.find(workflow_name);
    if (it == pipelines_.end())
        return;

    auto *state  = it->second.get();
    auto &param  = state->param;
    bool offline = state->pipeline->isOffline();

    SPDLOG_INFO("Workflow {} executing{}...", workflow_name, offline ? " (offline)" : "");

    if (!offline)
    {
        if (param.trigger_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(param.trigger_delay_ms));

        if (param.exposure_time > 0)
        {
            auto *cam = CameraManager::getInstance().getCamera(param.camera_name);
            if (cam)
                cam->setExposureTime(param.exposure_time);
        }
    }

    if (!state->pipeline->capture())
    {
        SPDLOG_ERROR("Workflow {} capture failed", workflow_name);
        state->state = State::IDLE;
        return;
    }

    auto &ctx = state->pipeline->context();
    emit sign_frameCaptured(param.camera_name, ctx.image);

    auto result = state->pipeline->process();

    {
        std::lock_guard lk(state->frame_mutex);
        state->pipeline->clearOfflineImage();
    }

    SPDLOG_INFO("Workflow {} result: {}", workflow_name, result.pass ? "OK" : "NG");
    emit sign_inspectionFinished(workflow_name, param.camera_name, ctx.display_image, result);

    state->state = State::HOLDING_RESULT;

    if (!offline && !param.comm_name.empty())
    {
        auto &comm = CommManager::getInstance();
        QMetaObject::invokeMethod(&comm,
            [&comm, param, pass = result.pass]()
            { comm.writeSingleCoil(param.comm_name, pass ? param.do_ok_addr : param.do_ng_addr, true); },
            Qt::QueuedConnection);

        auto start = std::chrono::steady_clock::now();
        while (running_)
        {
            auto di        = CommManager::getInstance().diState();
            bool di_cleared = (param.trigger_di_addr < 8) ? !di[param.trigger_di_addr] : true;
            auto elapsed   = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start).count();

            if (di_cleared && elapsed >= param.result_hold_ms)
                break;
            if (elapsed > 5000)
            {
                SPDLOG_WARN("Workflow {} DI clear timeout", workflow_name);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        QMetaObject::invokeMethod(&comm,
            [&comm, param]()
            {
                comm.writeSingleCoil(param.comm_name, param.do_ok_addr, false);
                comm.writeSingleCoil(param.comm_name, param.do_ng_addr, false);
            }, Qt::QueuedConnection);
    }

    state->state = State::IDLE;
}
