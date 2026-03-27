#include "draw_roi_algorithm.h"
#include "ui/camera_view_widget.h"
#include <spdlog/spdlog.h>
#include <QMessageBox>

void DrawROIAlgorithm::configure(CameraViewWidget *view, QWidget *parent,
                                  nlohmann::json &params)
{
    if (!view || !view->halconWindow())
    {
        QMessageBox::warning(parent, "DrawROI", "No camera window available");
        return;
    }

    try
    {
        double r1, c1, r2, c2;
        view->halconWindow()->SetColor("green");
        view->halconWindow()->DrawRectangle1(&r1, &c1, &r2, &c2);

        row1_ = r1;
        col1_ = c1;
        row2_ = r2;
        col2_ = c2;
        roi_set_ = true;

        params["row1"] = row1_;
        params["col1"] = col1_;
        params["row2"] = row2_;
        params["col2"] = col2_;

        spdlog::info("DrawROI configured: ({},{}) - ({},{})", row1_, col1_, row2_, col2_);
    }
    catch (const HalconCpp::HException &e)
    {
        spdlog::error("DrawROI configure failed: {}", e.ErrorMessage().Text());
    }
}

void DrawROIAlgorithm::loadParams(const nlohmann::json &params)
{
    if (params.contains("row1") && params.contains("col1") &&
        params.contains("row2") && params.contains("col2"))
    {
        row1_ = params["row1"].get<double>();
        col1_ = params["col1"].get<double>();
        row2_ = params["row2"].get<double>();
        col2_ = params["col2"].get<double>();
        roi_set_ = true;
    }
}

bool DrawROIAlgorithm::process(NodeContext &ctx)
{
    if (!roi_set_)
    {
        spdlog::warn("DrawROI: ROI not set, skipping crop");
        return true;
    }

    try
    {
        HalconCpp::HObject cropped;
        HalconCpp::CropRectangle1(ctx.image, &cropped,
                                   static_cast<int>(row1_), static_cast<int>(col1_),
                                   static_cast<int>(row2_), static_cast<int>(col2_));
        ctx.image = cropped;

        // display_image 上画 ROI 框标识裁剪区域
        ctx.data["roi_row1"] = row1_;
        ctx.data["roi_col1"] = col1_;
        ctx.data["roi_row2"] = row2_;
        ctx.data["roi_col2"] = col2_;

        return true;
    }
    catch (const HalconCpp::HException &e)
    {
        spdlog::error("DrawROI process failed: {}", e.ErrorMessage().Text());
        ctx.result.pass = false;
        ctx.result.detail += "ROI crop failed; ";
        return false;
    }
}
