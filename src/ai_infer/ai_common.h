#pragma once
#include <opencv2/core.hpp>
#include <variant>
namespace AIInfer
{
    struct Detection
    {
        cv::Rect box;
        float conf;
        int cls;
    };

    struct DetectionObb
    {
        Detection detection;
        float angle;
    };

    using DetectionResult = std::variant<std::vector<Detection>, std::vector<DetectionObb>>;

    struct ObjectGroup
    {
        int id;
        DetectionResult dets;
    };

    // letterbox 变换信息，用于将检测坐标映射回原图
    struct LetterBoxInfo
    {
        float scale; // 缩放比例
        int pad_x;   // x 方向 padding（左侧）
        int pad_y;   // y 方向 padding（上侧）
    };

    struct TensorData
    {
        void *data;
        size_t size;
        std::vector<int> shape;
    };

    enum class InputDimensionType
    {
        DYNAMIC
    };

    enum class TaskType
    {
        YOLO_DET,
        YOLO_OBB
    };

    enum class EngineType
    {
        OPENVINO
    };

}
