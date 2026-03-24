#pragma once

#include <string>
#include <opencv2/core.hpp>
#include <memory>
#include <vector>

namespace via {

struct AiDetection {
    int class_id = -1;
    std::string label;
    float confidence = 0.0f;
    cv::Rect bbox;
};

class AiEngine {
public:
    static AiEngine& instance();

    bool loadModel(const std::string& model_path, const std::string& device = "CPU");
    void unloadModel();
    bool isLoaded() const { return loaded_; }

    bool infer(const cv::Mat& image, std::vector<AiDetection>& detections,
               float conf_threshold = 0.5f);

private:
    AiEngine() = default;
    bool loaded_ = false;

    // OpenVINO handles (forward declared, actual implementation deferred)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace via
