#pragma once

#include "algorithm_interface.h"

/// 绘制 ROI 工具：配置时在 HWindow 上交互画矩形，运行时裁剪图像
class DrawROIAlgorithm : public IAlgorithm
{
public:
    std::string name() const override { return "DrawROI"; }
    std::string category() const override { return AlgorithmCategory::TOOL; }

    void configure(CameraViewWidget *view, QWidget *parent,
                   nlohmann::json &params) override;
    void loadParams(const nlohmann::json &params) override;
    bool process(NodeContext &ctx) override;

private:
    double row1_ = 0, col1_ = 0, row2_ = 0, col2_ = 0;
    bool roi_set_ = false;
};
