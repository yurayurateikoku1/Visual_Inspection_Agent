#pragma once
#include "idetector.h"
#include "ai_infer/yolo.h"
#include <memory>

/// @brief 端子检测器
///        算法链：YOLO 目标检测
class TerminalDetector : public IDetector
{
public:
    explicit TerminalDetector(const TerminalParam &param);
    void detect(NodeContext &ctx) override;

private:
    std::unique_ptr<AIInfer::YoloDetector> yolo_;
};
