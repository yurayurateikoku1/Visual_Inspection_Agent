#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include "ai_common.h"

namespace AIInfer
{
    class InferBackend
    {
    public:
        virtual ~InferBackend() = default;
        virtual int init(const std::string &model_path, InputDimensionType input_type) = 0;
        virtual int infer(const cv::Mat &image, std::vector<TensorData> &outputs) = 0;
        virtual void clean() = 0;

        bool getISDynamic() const { return input_type_ == InputDimensionType::DYNAMIC; }
        int getInputWidth() const { return input_width_; }
        int getInputHeight() const { return input_height_; }
        int getInputChannels() const { return input_channels_; }

    protected:
        InputDimensionType input_type_ = InputDimensionType::DYNAMIC;
        int input_width_ = 0;
        int input_height_ = 0;
        int input_channels_ = 3;
    };
}
