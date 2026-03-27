#include "inspection_pipeline.h"
#include "camera/camera_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>

InspectionPipeline::InspectionPipeline(const WorkflowParam &param)
    : param_(param) {}

InspectionPipeline::~InspectionPipeline() = default;

void InspectionPipeline::build()
{
    algorithms_.clear();

    for (size_t i = 0; i < param_.algorithm_ids.size(); ++i)
    {
        auto algo = createAlgorithm(param_.algorithm_ids[i]);
        if (!algo)
        {
            spdlog::warn("Algorithm {} not found, skipping", param_.algorithm_ids[i]);
            continue;
        }

        // 从已保存参数恢复状态
        if (i < param_.algorithm_params.size() && !param_.algorithm_params[i].is_null())
            algo->loadParams(param_.algorithm_params[i]);

        algorithms_.push_back(std::move(algo));
    }

    spdlog::info("Pipeline {} built: {} algorithms", param_.name, algorithms_.size());
}

void InspectionPipeline::setOfflineImage(const HalconCpp::HObject &image)
{
    offline_image_ = image;
}

void InspectionPipeline::clearOfflineImage()
{
    offline_image_.Clear();
}

bool InspectionPipeline::capture()
{
    ctx_ = NodeContext{};
    ctx_.camera_name = param_.camera_name;
    ctx_.result.pass = true;

    // 离线模式
    if (offline_image_.IsInitialized())
    {
        ctx_.image = offline_image_;
        ctx_.display_image = offline_image_;
        return true;
    }

    // 在线模式：从相机采集
    auto *cam = CameraManager::getInstance().getCamera(param_.camera_name);
    if (!cam)
    {
        spdlog::error("Pipeline {}: camera {} not found", param_.name, param_.camera_name);
        return false;
    }
    if (!cam->grabOne(ctx_.image))
    {
        spdlog::error("Pipeline {}: grabOne failed for {}", param_.name, param_.camera_name);
        return false;
    }
    ctx_.display_image = ctx_.image;
    return true;
}

InspectionResult InspectionPipeline::process()
{
    // 顺序执行算法链
    for (auto &algo : algorithms_)
    {
        if (!algo->process(ctx_))
            spdlog::error("Algorithm {} failed", algo->name());
    }

    // 打时间戳
    auto now = std::chrono::system_clock::now();
    ctx_.result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   now.time_since_epoch())
                                   .count();

    spdlog::info("Pipeline {}: camera={} pass={} detail={}",
                 param_.name, ctx_.camera_name, ctx_.result.pass, ctx_.result.detail);

    return ctx_.result;
}

std::vector<IAlgorithm *> InspectionPipeline::algorithms()
{
    std::vector<IAlgorithm *> ptrs;
    for (auto &a : algorithms_)
        ptrs.push_back(a.get());
    return ptrs;
}
