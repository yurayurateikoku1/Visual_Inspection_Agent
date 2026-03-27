#pragma once

#include "infer_backend.h"
#include <memory>
#include <openvino/openvino.hpp>

namespace AIInfer
{
    class OpenVINOInfer : public InferBackend
    {
    public:
        int init(const std::string &model_path, InputDimensionType input_type) override;
        int infer(const cv::Mat &image, std::vector<TensorData> &outputs) override;
        void clean() override;

    private:
        void compileAndCreateRequest();
        void reshapeIfNeeded(int h, int w, int c);

        ov::Core core_;
        std::shared_ptr<ov::Model> model_;
        ov::CompiledModel compiled_model_;
        ov::InferRequest infer_request_;

        size_t output_count_ = 0;
        std::vector<float> input_buffer_;
        std::vector<uint8_t> pixel_buffer_;
        int last_buffer_size_ = 0;
        int last_input_h_ = 0;
        int last_input_w_ = 0;
        bool is_dynamic_ = true;
        bool compiled_ = false;
    };
}
