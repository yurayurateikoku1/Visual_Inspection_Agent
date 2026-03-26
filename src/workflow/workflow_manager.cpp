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
                    wp.id, wp.camera_id, wp.trigger_di_addr, wp.do_ok_addr, wp.do_ng_addr);

        pipelines_[wp.id] = std::move(state);
    }
}

void WorkflowManager::startAll()
{
    if (running_)
        return;

    // 清除所有 DO 输出
    auto &comm = CommManager::getInstance();
    for (auto &[id, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm, [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr, comm_id = state->param.comm_id]()
                                  {
            comm.writeSingleCoil(comm_id, ok, false);
            comm.writeSingleCoil(comm_id, ng, false); }, Qt::QueuedConnection);
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
    for (auto &[id, state] : pipelines_)
    {
        QMetaObject::invokeMethod(&comm, [&comm, ok = state->param.do_ok_addr, ng = state->param.do_ng_addr, comm_id = state->param.comm_id]()
                                  {
            comm.writeSingleCoil(comm_id, ok, false);
            comm.writeSingleCoil(comm_id, ng, false); }, Qt::QueuedConnection);
    }

    emit sign_runningChanged(false);
    SPDLOG_INFO("WorkflowManager stopped");
}

void WorkflowManager::triggerOnce(const std::string &workflow_id)
{
    auto it = pipelines_.find(workflow_id);
    if (it == pipelines_.end())
    {
        SPDLOG_WARN("Workflow {} not found", workflow_id);
        return;
    }

    auto *state = it->second.get();
    if (state->busy)
    {
        SPDLOG_WARN("Workflow {} is busy, skipping manual trigger", workflow_id);
        return;
    }

    // 在 Taskflow 线程池中异步执行
    state->busy = true;
    executor_.silent_async([this, workflow_id]()
                           { executeWorkflow(workflow_id); });
}

void WorkflowManager::slot_onIOStateUpdated()
{
    if (!running_)
        return;

    auto di = CommManager::getInstance().diState();

    for (auto &[id, state] : pipelines_)
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
            SPDLOG_INFO("DI{} rising edge → trigger workflow {}", addr, id);
            state->busy = true;
            executor_.silent_async([this, wf_id = id]()
                                   { executeWorkflow(wf_id); });
        }
    }
}

void WorkflowManager::executeWorkflow(const std::string &workflow_id)
{
    auto it = pipelines_.find(workflow_id);
    if (it == pipelines_.end())
        return;

    auto *state = it->second.get();
    auto &param = state->param;
    auto &comm = CommManager::getInstance();

    SPDLOG_INFO("Workflow {} executing...", workflow_id);

    // 1. 触发延时（对应 C# TrigDelay）
    if (param.trigger_delay_ms > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(param.trigger_delay_ms));
    }

    // 2. 覆盖曝光参数（对应 C# 不同检测切换曝光）
    if (param.exposure_time > 0)
    {
        auto *cam = CameraManager::getInstance().getCamera(param.camera_id);
        if (cam)
        {
            cam->setExposureTime(param.exposure_time);
        }
    }

    // 3. 采集图像
    if (!state->pipeline->capture())
    {
        SPDLOG_ERROR("Workflow {} capture failed", workflow_id);
        state->busy = false;
        return;
    }

    // 4. 发送原图到 UI 显示
    auto &ctx = state->pipeline->context();
    emit sign_frameCaptured(param.camera_id, ctx.image);

    // 5. 执行算法链 + 结果节点（在 display_image 上叠加检测框）
    auto result = state->pipeline->process(executor_);

    // 6. 写 DO 输出（对应 C# OutY[0]=OK, OutY[1]=NG）
    if (!param.comm_id.empty())
    {
        QMetaObject::invokeMethod(&comm, [&comm, param, pass = result.pass]()
                                  { comm.writeSingleCoil(param.comm_id, pass ? param.do_ok_addr : param.do_ng_addr, true); }, Qt::QueuedConnection);
    }

    SPDLOG_INFO("Workflow {} result: {}", workflow_id, result.pass ? "OK" : "NG");
    emit sign_inspectionFinished(workflow_id, param.camera_id, ctx.display_image, result);

    // 5. 等待 DI 信号恢复（对应 C# step 12: 等 InX[0]==0）
    //    然后清除 DO 输出
    if (!param.comm_id.empty())
    {
        // 等待 DI 恢复 + 保持时间
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

            // 超时保护 5s
            if (elapsed > 5000)
            {
                SPDLOG_WARN("Workflow {} DI clear timeout", workflow_id);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 清除 DO
        QMetaObject::invokeMethod(&comm, [&comm, param]()
                                  {
            comm.writeSingleCoil(param.comm_id, param.do_ok_addr, false);
            comm.writeSingleCoil(param.comm_id, param.do_ng_addr, false); }, Qt::QueuedConnection);
    }

    state->busy = false;
}
