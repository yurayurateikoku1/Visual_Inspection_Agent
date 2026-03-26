#pragma once

#include <string>
#include <halconcpp/HalconCpp.h>
#include <memory>
#include <vector>
#include "app/common.h"

namespace via {

struct AiDetection {
    int class_id = -1;
    std::string label;
    float confidence = 0.0f;
    DefectRect bbox;
};

class AiEngine {
public:
    static AiEngine& instance();

    bool loadModel(const std::string& model_path, const std::string& device = "CPU");
    void unloadModel();
    bool isLoaded() const { return loaded_; }

    bool infer(const HalconCpp::HObject& image, std::vector<AiDetection>& detections,
               float conf_threshold = 0.5f);

private:
    AiEngine() = default;
    bool loaded_ = false;

    // OpenVINO handles (forward declared, actual implementation deferred)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace via
