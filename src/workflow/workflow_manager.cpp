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
    // 监听 CommManager IO 状态更新，检测 DI 上升沿
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

    for (auto &wp : AppContext::getInstance().workflowParams())
    {
        auto state = std::make_unique<PipelineState>();
        state->param = wp;
        state->pipeline = std::make_unique<InspectionPipeline>(wp);
        state->pipeline->build();
        state->last_di_state = false;
        state->busy = false;

        SPDLOG_INFO("Workflow {} built: camera={}, DI={}, DO_OK={}, DO_NG={}",
                    wp.name, wp.camera_name, wp.trigger_di_addr, wp.do_ok_addr, wp.do_ng_addr);

        pipelines_[wp.name] = std::move(state);
    }
}

void WorkflowManager::startAll()
{
    if (running_)
        return;

    // 清除所有 DO 输出
    auto &comm = CommManager::getInstance();
    for (auto &[name, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm, [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr, comm_name = state->param.comm_name]()
                                  {
            comm.writeSingleCoil(comm_name, ok, false);
            comm.writeSingleCoil(comm_name, ng, false); }, Qt::QueuedConnection);
        state->busy = false;
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

    // 清除所有 DO 输出
    auto &comm = CommManager::getInstance();
    for (auto &[name, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm, [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr, comm_name = state->param.comm_name]()
                                  {
            comm.writeSingleCoil(comm_name, ok, false);
            comm.writeSingleCoil(comm_name, ng, false); }, Qt::QueuedConnection);
    }

    emit sign_runningChanged(false);
    SPDLOG_INFO("WorkflowManager stopped");
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
    if (state->busy)
    {
        SPDLOG_WARN("Workflow {} is busy, skipping manual trigger", workflow_name);
        return;
    }

    state->busy = true;
    executor_.silent_async([this, workflow_name]()
                           { executeWorkflow(workflow_name); });
}

void WorkflowManager::rebuildWorkflow(const std::string &camera_name)
{
    // 找到该相机对应的 Pipeline
    for (auto &[name, state] : pipelines_)
    {
        if (state->param.camera_name != camera_name)
            continue;

        if (state->busy)
        {
            SPDLOG_WARN("Workflow {} is busy, cannot rebuild now", name);
            return;
        }

        // 从 AppContext 重新读取最新配置
        for (auto &wp : AppContext::getInstance().workflowParams())
        {
            if (wp.camera_name == camera_name)
            {
                state->param = wp;
                state->pipeline = std::make_unique<InspectionPipeline>(wp);
                state->pipeline->build();
                SPDLOG_INFO("Workflow {} rebuilt: {} algorithms", name, wp.algorithm_ids.size());
                break;
            }
        }
        return;
    }
}

void WorkflowManager::setOfflineImage(const std::string &workflow_name, const HalconCpp::HObject &image)
{
    auto it = pipelines_.find(workflow_name);
    if (it != pipelines_.end())
        it->second->pipeline->setOfflineImage(image);
}

void WorkflowManager::clearOfflineImage(const std::string &workflow_name)
{
    auto it = pipelines_.find(workflow_name);
    if (it != pipelines_.end())
        it->second->pipeline->clearOfflineImage();
}

void WorkflowManager::slot_onIOStateUpdated()
{
    if (!running_)
        return;

    auto di = CommManager::getInstance().diState();

    for (auto &[name, state] : pipelines_)
    {
        uint16_t addr = state->param.trigger_di_addr;
        if (addr >= 8)
            continue;

        bool current_di = di[addr];
        bool last_di = state->last_di_state;
        state->last_di_state = current_di;

        // 检测上升沿：从 false → true
        if (current_di && !last_di && !state->busy)
        {
            SPDLOG_INFO("DI{} rising edge → trigger workflow {}", addr, name);
            state->busy = true;
            executor_.silent_async([this, wf_name = name]()
                                   { executeWorkflow(wf_name); });
        }
    }
}

void WorkflowManager::executeWorkflow(const std::string &workflow_name)
{
    auto it = pipelines_.find(workflow_name);
    if (it == pipelines_.end())
        return;

    auto *state = it->second.get();
    auto &param = state->param;
    bool offline = state->pipeline->isOffline();

    SPDLOG_INFO("Workflow {} executing{}...", workflow_name, offline ? " (offline)" : "");

    // ── 在线模式独有：触发延时 + 覆盖曝光 ──
    if (!offline)
    {
        if (param.trigger_delay_ms > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(param.trigger_delay_ms));
        }
        if (param.exposure_time > 0)
        {
            auto *cam = CameraManager::getInstance().getCamera(param.camera_name);
            if (cam)
                cam->setExposureTime(param.exposure_time);
        }
    }

    // ── 采集图像（离线时自动使用注入的图像） ──
    if (!state->pipeline->capture())
    {
        SPDLOG_ERROR("Workflow {} capture failed", workflow_name);
        state->busy = false;
        return;
    }

    // ── 发送原图到 UI 显示 ──
    auto &ctx = state->pipeline->context();
    emit sign_frameCaptured(param.camera_name, ctx.image);

    // ── 执行算法链 ──
    auto result = state->pipeline->process(executor_);

    SPDLOG_INFO("Workflow {} result: {}", workflow_name, result.pass ? "OK" : "NG");
    emit sign_inspectionFinished(workflow_name, param.camera_name, ctx.display_image, result);

    // ── 在线模式独有：DO 输出 + 等待 DI 恢复 ──
    if (!offline && !param.comm_name.empty())
    {
        auto &comm = CommManager::getInstance();

        // 写 DO 输出
        QMetaObject::invokeMethod(&comm, [&comm, param, pass = result.pass]()
                                  { comm.writeSingleCoil(param.comm_name, pass ? param.do_ok_addr : param.do_ng_addr, true); }, Qt::QueuedConnection);

        // 等待 DI 信号恢复
        auto start = std::chrono::steady_clock::now();
        while (running_)
        {
            auto di = CommManager::getInstance().diState();
            bool di_cleared = (param.trigger_di_addr < 8) ? !di[param.trigger_di_addr] : true;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();

            if (di_cleared && elapsed >= param.result_hold_ms)
                break;

            if (elapsed > 5000)
            {
                SPDLOG_WARN("Workflow {} DI clear timeout", workflow_name);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 清除 DO
        QMetaObject::invokeMethod(&comm, [&comm, param]()
                                  {
            comm.writeSingleCoil(param.comm_name, param.do_ok_addr, false);
            comm.writeSingleCoil(param.comm_name, param.do_ng_addr, false); }, Qt::QueuedConnection);
    }

    state->busy = false;
}
