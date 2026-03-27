#pragma once

#include "algorithm_interface.h"
#include "ai_infer/yolo.h"
#include <memory>

/// YOLO 端子检测算法
class YoloTerminalAlgorithm : public IAlgorithm
{
public:
    std::string name() const override { return "YoloTerminal"; }
    std::string category() const override { return AlgorithmCategory::AI_INFER; }

    void configure(CameraViewWidget *view, QWidget *parent,
                   nlohmann::json &params) override;
    void loadParams(const nlohmann::json &params) override;
    bool process(NodeContext &ctx) override;

private:
    void initDetector();

    std::unique_ptr<AIInfer::YoloDetector> detector_;
    nlohmann::json params_;
};
