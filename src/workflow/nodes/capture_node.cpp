#include "capture_node.h"
#include "camera/camera_manager.h"
#include <spdlog/spdlog.h>

CaptureNode::CaptureNode(const std::string &camera_name) : camera_name_(camera_name) {}

bool CaptureNode::execute(NodeContext &ctx)
{
    auto *cam = CameraManager::getInstance().getCamera(camera_name_);
    if (!cam)
    {
        spdlog::error("CaptureNode: camera {} not found", camera_name_);
        return false;
    }

    ctx.camera_name = camera_name_;
    if (!cam->grabOne(ctx.image))
    {
        spdlog::error("CaptureNode: grabOne failed for {}", camera_name_);
        return false;
    }

    // 复制一份用于叠加检测结果显示
    ctx.display_image = ctx.image;

    spdlog::debug("CaptureNode: captured from {}", camera_name_);
    return true;
}
