#include "openvino_infer.h"

int AIInfer::OpenVINOInfer::init(const std::string &model_path, InputDimensionType input_type)
{
    return 0;
}

int AIInfer::OpenVINOInfer::infer(const cv::Mat &image, std::vector<TensorData> &outputs)
{
    return 0;
}

void AIInfer::OpenVINOInfer::clean()
{
}
