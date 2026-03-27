#include "algorithm_interface.h"
#include "draw_roi_algorithm.h"
#include "yolo_terminal_algorithm.h"
#include <spdlog/spdlog.h>

std::unique_ptr<IAlgorithm> createAlgorithm(const std::string &id)
{
    if (id == "DrawROI")
        return std::make_unique<DrawROIAlgorithm>();
    if (id == "YoloTerminal")
        return std::make_unique<YoloTerminalAlgorithm>();

    spdlog::error("Unknown algorithm id: {}", id);
    return nullptr;
}
