#include "terminal_detector.h"
#include <spdlog/spdlog.h>

TerminalDetector::TerminalDetector(const TerminalParam &param)
{
    if (!param.enabled || param.model_path.empty())
        return;

    AIInfer::YOLOSettings settings;
    settings.model_path      = param.model_path;
    settings.score_threshold = param.score_threshold;
    settings.nms_threshold   = param.nms_threshold;
    settings.end2end         = param.end2end;
    settings.task_type       = (param.task_type == "YOLO_OBB")
                                   ? AIInfer::TaskType::YOLO_OBB
                                   : AIInfer::TaskType::YOLO_DET;
    settings.input_type  = AIInfer::InputDimensionType::DYNAMIC;
    settings.engine_type = AIInfer::EngineType::OPENVINO;

    yolo_ = std::make_unique<AIInfer::YoloDetector>(settings);
    spdlog::info("TerminalDetector: model loaded: {}", param.model_path);
}

void TerminalDetector::detect(NodeContext &ctx)
{
    if (!yolo_)
    {
        ctx.result.pass = false;
        return;
    }

    try
    {
        auto det_result = yolo_->detect(ctx.image);

        bool has_det = std::visit([](const auto &dets) { return !dets.empty(); }, det_result);
        if (!has_det)
        {
            ctx.result.pass = false;
            return;
        }

        std::visit([&](const auto &dets)
                   {
                       for (const auto &d : dets)
                       {
                           Defect def;
                           const AIInfer::Detection *base;
                           if constexpr (std::is_same_v<std::decay_t<decltype(d)>, AIInfer::Detection>)
                               base = &d;
                           else
                               base = &d.detection;

                           def.label      = "cls" + std::to_string(base->cls);
                           def.confidence = base->conf;
                           def.col1       = static_cast<float>(base->box.x);
                           def.row1       = static_cast<float>(base->box.y);
                           def.col2       = static_cast<float>(base->box.x + base->box.width);
                           def.row2       = static_cast<float>(base->box.y + base->box.height);
                           ctx.result.defects.push_back(def);
                       }
                   }, det_result);
    }
    catch (const std::exception &e)
    {
        spdlog::error("TerminalDetector: detect failed: {}", e.what());
        ctx.result.pass = false;
    }
}
