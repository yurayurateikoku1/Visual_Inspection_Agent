#include "inspection_pipeline.h"
#include "camera/camera_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>

InspectionPipeline::InspectionPipeline(const WorkflowParam &param)
    : param_(param) {}

InspectionPipeline::~InspectionPipeline() = default;

void InspectionPipeline::build()
{
    detector_.reset();

    if (param_.yolo.enabled && !param_.yolo.model_path.empty())
    {
        try
        {
            AIInfer::YOLOSettings settings;
            settings.model_path      = param_.yolo.model_path;
            settings.score_threshold = param_.yolo.score_threshold;
            settings.nms_threshold   = param_.yolo.nms_threshold;
            settings.end2end         = param_.yolo.end2end;
            settings.task_type       = (param_.yolo.task_type == "YOLO_OBB")
                                           ? AIInfer::TaskType::YOLO_OBB
                                           : AIInfer::TaskType::YOLO_DET;
            settings.input_type  = AIInfer::InputDimensionType::DYNAMIC;
            settings.engine_type = AIInfer::EngineType::OPENVINO;

            detector_ = std::make_unique<AIInfer::YoloDetector>(settings);
            spdlog::info("Pipeline {}: YOLO detector loaded: {}", param_.name, settings.model_path);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Pipeline {}: YOLO init failed: {}", param_.name, e.what());
        }
    }

    spdlog::info("Pipeline {} built: roi={} yolo={}", param_.name,
                 param_.roi.enabled, param_.yolo.enabled);
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
    ctx_             = NodeContext{};
    ctx_.camera_name = param_.camera_name;
    ctx_.result.pass = true;

    if (offline_image_.IsInitialized())
    {
        ctx_.image         = offline_image_;
        ctx_.display_image = offline_image_;
        return true;
    }

    auto *cam = CameraManager::getInstance().getCamera(param_.camera_name);
    if (!cam)
    {
        spdlog::error("Pipeline {}: camera {} not found", param_.name, param_.camera_name);
        return false;
    }
    if (!cam->grabOne(ctx_.image))
    {
        spdlog::error("Pipeline {}: grabOne failed", param_.name);
        return false;
    }
    ctx_.display_image = ctx_.image;
    return true;
}

InspectionResult InspectionPipeline::process()
{
    if (param_.roi.enabled)
        processRoi();

    if (param_.yolo.enabled)
        processYolo();

    auto now = std::chrono::system_clock::now();
    ctx_.result.timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    spdlog::info("Pipeline {}: pass={} detail={}",
                 param_.name, ctx_.result.pass, ctx_.result.detail);
    return ctx_.result;
}

void InspectionPipeline::processRoi()
{
    const auto &roi = param_.roi;
    try
    {
        HalconCpp::HObject cropped;
        HalconCpp::CropRectangle1(ctx_.image, &cropped,
                                   static_cast<int>(roi.row1), static_cast<int>(roi.col1),
                                   static_cast<int>(roi.row2), static_cast<int>(roi.col2));
        ctx_.image         = cropped;
        ctx_.display_image = cropped;
    }
    catch (const HalconCpp::HException &e)
    {
        spdlog::error("Pipeline {}: ROI crop failed: {}", param_.name, e.ErrorMessage().Text());
        ctx_.result.pass    = false;
        ctx_.result.detail += "ROI crop failed; ";
    }
}

void InspectionPipeline::processYolo()
{
    if (!detector_)
    {
        ctx_.result.pass    = false;
        ctx_.result.detail += "YOLO detector not initialized; ";
        return;
    }

    try
    {
        auto det_result = detector_->detect(ctx_.image);

        bool has_det = std::visit([](const auto &dets) { return !dets.empty(); }, det_result);
        if (!has_det)
        {
            ctx_.result.pass    = false;
            ctx_.result.detail += "no object detected; ";
            ctx_.result.confidence = 0.0;
            return;
        }

        double max_conf = 0.0;
        int count       = 0;
        std::visit([&](const auto &dets)
                   {
                       count = static_cast<int>(dets.size());
                       for (const auto &d : dets)
                       {
                           double c;
                           if constexpr (std::is_same_v<std::decay_t<decltype(d)>, AIInfer::Detection>)
                               c = d.conf;
                           else
                               c = d.detection.conf;
                           if (c > max_conf) max_conf = c;
                       }
                   },
                   det_result);

        ctx_.result.confidence = max_conf;
        ctx_.result.detail    += "detected " + std::to_string(count) + " objects; ";
    }
    catch (const std::exception &e)
    {
        spdlog::error("Pipeline {}: YOLO failed: {}", param_.name, e.what());
        ctx_.result.pass    = false;
        ctx_.result.detail += std::string("YOLO exception: ") + e.what() + "; ";
    }
}
