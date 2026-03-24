#include "capture_node.h"
#include "camera/camera_manager.h"
#include <spdlog/spdlog.h>

CaptureNode::CaptureNode(const std::string &camera_id) : camera_id_(camera_id) {}

bool CaptureNode::execute(NodeContext &ctx)
{
    auto *cam = CameraManager::getInstance().getCamera(camera_id_);
    if (!cam)
    {
        spdlog::error("CaptureNode: camera {} not found", camera_id_);
        return false;
    }

    ctx.camera_id = camera_id_;
    if (!cam->grabOne(ctx.image))
    {
        spdlog::error("CaptureNode: grabOne failed for {}", camera_id_);
        return false;
    }

    spdlog::debug("CaptureNode: captured {}x{} from {}", ctx.image.cols, ctx.image.rows, camera_id_);
    return true;
}
