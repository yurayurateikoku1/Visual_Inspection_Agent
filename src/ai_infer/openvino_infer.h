#pragma once

#include "infer_backend.h"

namespace AIInfer
{
    class OpenVINOInfer : public InferBackend
    {
    public:
        int init(const std::string &model_path, InputDimensionType input_type) override;
        int infer(const cv::Mat &image, std::vector<TensorData> &outputs) override;
        void clean() override;
    };
}