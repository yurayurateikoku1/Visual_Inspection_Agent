#include "yolo.h"
#include "openvino_infer.h"
namespace AIInfer
{
    YoloDetector::YoloDetector(const YOLOSettings &settings)
        : settings_(settings)
    {
        switch (settings_.engine_type)
        {
        case EngineType::OPENVINO:
            backend_ = std::make_unique<OpenVINOInfer>();
            break;
        default:
            throw std::runtime_error("Unsupported engine type");
        }
        post_processor_ = createPostProcessor(settings_.task_type, settings_.end2end);
        backend_->init(settings_.model_path, settings_.input_type);
    }

    DetectionResult YoloDetector::detect(const cv::Mat &image)
    {
        LetterBoxInfo lb_info;
        cv::Mat input = letterBox(image,
                                  backend_->getInputWidth(),
                                  backend_->getInputHeight(),
                                  lb_info);

        std::vector<TensorData> outputs;
        backend_->infer(input, outputs);

        return post_processor_->process(outputs[0], lb_info,
                                        settings_.score_threshold,
                                        settings_.nms_threshold);
    }

    DetectionResult YoloDetector::detect(const HalconCpp::HObject &image)
    {
        // 从 HObject 提取图像尺寸和通道信息
        using namespace HalconCpp;
        HTuple channels;
        CountChannels(image, &channels);
        int c = static_cast<int>(channels.I());

        HTuple pointer, type, width, height;
        if (c == 1)
        {
            GetImagePointer1(image, &pointer, &type, &width, &height);
            cv::Mat gray(height.I(), width.I(), CV_8UC1,
                         reinterpret_cast<void *>(pointer.L()));
            cv::Mat bgr;
            cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
            return detect(bgr);
        }
        else
        {
            HTuple pr, pg, pb;
            GetImagePointer3(image, &pr, &pg, &pb, &type, &width, &height);
            int h = height.I(), w = width.I();

            cv::Mat mat_r(h, w, CV_8UC1, reinterpret_cast<void *>(pr.L()));
            cv::Mat mat_g(h, w, CV_8UC1, reinterpret_cast<void *>(pg.L()));
            cv::Mat mat_b(h, w, CV_8UC1, reinterpret_cast<void *>(pb.L()));

            cv::Mat bgr;
            cv::merge(std::vector<cv::Mat>{mat_b, mat_g, mat_r}, bgr);
            return detect(bgr);
        }
    }
}