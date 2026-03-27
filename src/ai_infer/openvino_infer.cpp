#include "openvino_infer.h"
#include "ai_utils.h"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace AIInfer
{

int OpenVINOInfer::init(const std::string &model_path, InputDimensionType input_type)
{
    try
    {
        input_type_ = input_type;
        model_ = core_.read_model(model_path);

        // 设置模型缓存目录
        std::filesystem::path cache_dir =
            std::filesystem::path(model_path).parent_path() / "ov_cache";
        core_.set_property("CPU", ov::cache_dir(cache_dir.string()));

        // 检测输入是否为动态维度
        is_dynamic_ = true;
        auto input_shape = model_->input(0).get_partial_shape();
        if (input_shape.is_static() && input_shape.rank().get_length() == 4)
        {
            input_height_ = static_cast<int>(input_shape[2].get_length());
            input_width_ = static_cast<int>(input_shape[3].get_length());
            input_channels_ = static_cast<int>(input_shape[1].get_length());
            is_dynamic_ = false;
        }

        if (!is_dynamic_)
        {
            compileAndCreateRequest();
        }
        // 动态维度延迟到首次 infer() 时 reshape 后编译

        SPDLOG_INFO("OpenVINOInfer init ok: {} (dynamic={})", model_path, is_dynamic_);
        return 0;
    }
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("OpenVINOInfer init exception: {}", e.what());
        return -1;
    }
}

void OpenVINOInfer::compileAndCreateRequest()
{
    ov::AnyMap props;
    props[ov::hint::performance_mode.name()] = ov::hint::PerformanceMode::LATENCY;
    props[ov::hint::enable_cpu_pinning.name()] = true;

    compiled_model_ = core_.compile_model(model_, "CPU", props);
    infer_request_ = compiled_model_.create_infer_request();
    output_count_ = compiled_model_.outputs().size();
    compiled_ = true;
}

void OpenVINOInfer::reshapeIfNeeded(int h, int w, int c)
{
    if (h == last_input_h_ && w == last_input_w_)
        return;

    model_->reshape({{model_->input(0).get_any_name(),
                       ov::Shape{1, static_cast<size_t>(c),
                                 static_cast<size_t>(h),
                                 static_cast<size_t>(w)}}});
    compileAndCreateRequest();

    last_input_h_ = h;
    last_input_w_ = w;
}

int OpenVINOInfer::infer(const cv::Mat &image, std::vector<TensorData> &outputs)
{
    try
    {
        int h = image.rows;
        int w = image.cols;
        int c = image.channels();
        int pixel_count = h * w * c;
        int tensor_size = 1 * c * h * w;

        // 动态模型：尺寸变化时 reshape 并重新编译
        if (is_dynamic_)
            reshapeIfNeeded(h, w, c);

        if (!compiled_)
        {
            SPDLOG_ERROR("OpenVINOInfer::infer: model not compiled");
            return -1;
        }

        // 尺寸变化时重新分配缓冲区
        if (last_buffer_size_ != tensor_size)
        {
            input_buffer_.resize(tensor_size);
            pixel_buffer_.resize(pixel_count);
            last_buffer_size_ = tensor_size;
        }

        // Mat → 连续像素 → NCHW RGB float
        if (image.isContinuous())
        {
            std::memcpy(pixel_buffer_.data(), image.data, pixel_count);
        }
        else
        {
            cv::Mat cont;
            image.copyTo(cont);
            std::memcpy(pixel_buffer_.data(), cont.data, pixel_count);
        }
        fillNCHW(pixel_buffer_.data(), input_buffer_.data(), h, w, c);

        // 构建输入张量并推理
        ov::Shape input_shape{1, static_cast<size_t>(c),
                              static_cast<size_t>(h),
                              static_cast<size_t>(w)};
        ov::Tensor input_tensor(ov::element::f32, input_shape, input_buffer_.data());
        infer_request_.set_input_tensor(input_tensor);

        infer_request_.infer();

        // 提取所有输出张量
        outputs.clear();
        for (size_t i = 0; i < output_count_; i++)
        {
            ov::Tensor out_tensor = infer_request_.get_output_tensor(i);
            const ov::Shape &dims = out_tensor.get_shape();
            float *raw = out_tensor.data<float>();
            size_t elem_count = out_tensor.get_size();

            TensorData td;
            td.data = raw;
            td.size = elem_count;
            td.shape.resize(dims.size());
            for (size_t j = 0; j < dims.size(); j++)
                td.shape[j] = static_cast<int>(dims[j]);

            outputs.push_back(td);
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("OpenVINOInfer::infer exception: {}", e.what());
        return -1;
    }
}

void OpenVINOInfer::clean()
{
    infer_request_ = {};
    compiled_model_ = {};
    model_.reset();
    compiled_ = false;
    output_count_ = 0;
    last_buffer_size_ = 0;
    last_input_h_ = 0;
    last_input_w_ = 0;
}

} // namespace AIInfer
