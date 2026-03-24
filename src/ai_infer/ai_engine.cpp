#include "ai_engine.h"
#include <spdlog/spdlog.h>

// TODO: 当集成 OpenVINO 时取消以下注释
// #include <openvino/openvino.hpp>

namespace via {

struct AiEngine::Impl {
    // ov::Core core;
    // ov::CompiledModel compiled_model;
    // ov::InferRequest infer_request;
    int input_w = 640;
    int input_h = 640;
};

AiEngine& AiEngine::instance() {
    static AiEngine inst;
    return inst;
}

bool AiEngine::loadModel(const std::string& model_path, const std::string& device) {
    impl_ = std::make_unique<Impl>();

    // TODO: OpenVINO 模型加载
    // impl_->core = ov::Core();
    // auto model = impl_->core.read_model(model_path);
    // impl_->compiled_model = impl_->core.compile_model(model, device);
    // impl_->infer_request = impl_->compiled_model.create_infer_request();

    spdlog::info("AiEngine: model loaded (stub) from {}", model_path);
    loaded_ = true;
    return true;
}

void AiEngine::unloadModel() {
    impl_.reset();
    loaded_ = false;
}

bool AiEngine::infer(const cv::Mat& image, std::vector<AiDetection>& detections,
                      float conf_threshold) {
    if (!loaded_ || !impl_) {
        spdlog::error("AiEngine: model not loaded");
        return false;
    }

    // TODO: 实际推理逻辑
    // 1. 预处理 image -> input tensor
    // 2. infer_request.infer()
    // 3. 后处理 output -> detections

    detections.clear();
    spdlog::debug("AiEngine: infer (stub) on {}x{}", image.cols, image.rows);
    return true;
}

} // namespace via
