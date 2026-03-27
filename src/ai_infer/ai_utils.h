#pragma once
#include <opencv2/opencv.hpp>
#include "ai_common.h"

namespace AIInfer
{
    // letterbox: 等比缩放到 target_w x target_h，居中 padding
    inline cv::Mat letterBox(const cv::Mat &input_image,
                             int target_w, int target_h,
                             LetterBoxInfo &info)
    {
        float scale_w = static_cast<float>(target_w) / input_image.cols;
        float scale_h = static_cast<float>(target_h) / input_image.rows;
        info.scale = std::min(scale_w, scale_h);

        int scaled_w = static_cast<int>(input_image.cols * info.scale);
        int scaled_h = static_cast<int>(input_image.rows * info.scale);

        info.pad_x = (target_w - scaled_w) / 2;
        info.pad_y = (target_h - scaled_h) / 2;

        cv::Mat resized;
        cv::resize(input_image, resized, cv::Size(scaled_w, scaled_h));

        cv::Mat output_image;
        cv::copyMakeBorder(resized, output_image,
                           info.pad_y, target_h - scaled_h - info.pad_y,
                           info.pad_x, target_w - scaled_w - info.pad_x,
                           cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));
        return output_image;
    }

    // 将 letterbox 坐标映射回原图
    inline void scaleBoxToOriginal(cv::Rect &box, const LetterBoxInfo &info)
    {
        box.x = static_cast<int>((box.x - info.pad_x) / info.scale);
        box.y = static_cast<int>((box.y - info.pad_y) / info.scale);
        box.width = static_cast<int>(box.width / info.scale);
        box.height = static_cast<int>(box.height / info.scale);
    }

    inline void NMSBoxes(const std::vector<Detection> &dets,
                         float conf_threshold, float nms_threshold,
                         std::vector<int> &indices)
    {
        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        for (const auto &d : dets)
        {
            if (d.conf < conf_threshold)
                continue;
            boxes.emplace_back(d.box);
            scores.emplace_back(d.conf);
        }
        cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, indices);
    }

    // OBB 旋转矩形 NMS
    inline void NMSBoxesRotated(const std::vector<DetectionObb> &dets,
                                float conf_threshold, float nms_threshold,
                                std::vector<int> &indices)
    {
        std::vector<cv::RotatedRect> boxes;
        std::vector<float> scores;
        for (const auto &d : dets)
        {
            if (d.detection.conf < conf_threshold)
                continue;
            // RotatedRect: 中心点 + 尺寸 + 角度（度）
            cv::RotatedRect rr(
                cv::Point2f(d.detection.box.x + d.detection.box.width / 2.0f,
                            d.detection.box.y + d.detection.box.height / 2.0f),
                cv::Size2f(d.detection.box.width, d.detection.box.height),
                d.angle * 180.0f / CV_PI); // 弧度 → 度
            boxes.push_back(rr);
            scores.push_back(d.detection.conf);
        }
        // NMSBoxes 的 RotatedRect 重载
        cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, indices);
    }

    // 将 letterbox 旋转框坐标映射回原图
    inline void scaleRotatedBoxToOriginal(cv::RotatedRect &rr, const LetterBoxInfo &info)
    {
        rr.center.x = (rr.center.x - info.pad_x) / info.scale;
        rr.center.y = (rr.center.y - info.pad_y) / info.scale;
        rr.size.width = rr.size.width / info.scale;
        rr.size.height = rr.size.height / info.scale;
    }

    // 随机颜色表，按类别索引取色
    static cv::Scalar getColor(int cls)
    {
        static const cv::Scalar colors[] = {
            {255, 0, 0},
            {0, 255, 0},
            {0, 0, 255},
            {255, 255, 0},
            {255, 0, 255},
            {0, 255, 255},
            {128, 0, 255},
            {255, 128, 0},
            {0, 128, 255},
            {128, 255, 0},
            {255, 0, 128},
            {0, 255, 128},
        };
        return colors[cls % 12];
    }

    // 画普通检测框
    inline void drawDetections(cv::Mat &image, const std::vector<Detection> &dets)
    {
        for (const auto &d : dets)
        {
            cv::Scalar color = getColor(d.cls);
            cv::rectangle(image, d.box, color, 2);

            char label[64];
            snprintf(label, sizeof(label), "cls:%d %.2f", d.cls, d.conf);
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::rectangle(image,
                          cv::Point(d.box.x, d.box.y - text_size.height - 4),
                          cv::Point(d.box.x + text_size.width, d.box.y),
                          color, -1);
            cv::putText(image, label, cv::Point(d.box.x, d.box.y - 2),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
        }
    }

    // 画 OBB 旋转框
    inline void drawOBBDetections(cv::Mat &image, const std::vector<DetectionObb> &dets)
    {
        for (const auto &d : dets)
        {
            cv::Scalar color = getColor(d.detection.cls);

            // 用 RotatedRect 画旋转框
            cv::RotatedRect rr(
                cv::Point2f(d.detection.box.x + d.detection.box.width / 2.0f,
                            d.detection.box.y + d.detection.box.height / 2.0f),
                cv::Size2f(d.detection.box.width, d.detection.box.height),
                d.angle * 180.0f / CV_PI);

            cv::Point2f pts[4];
            rr.points(pts);
            for (int j = 0; j < 4; j++)
                cv::line(image, pts[j], pts[(j + 1) % 4], color, 2);

            char label[64];
            snprintf(label, sizeof(label), "cls:%d %.2f", d.detection.cls, d.detection.conf);
            cv::putText(image, label, cv::Point(static_cast<int>(pts[1].x), static_cast<int>(pts[1].y) - 4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }
    }

    // 将 HWC BGR byte 数据转换为 NCHW RGB float 数据
    inline void fillNCHW(const uint8_t *src, float *dst, int h, int w, int c, bool normalize = true)
    {
        int hw = h * w;
        float scale = normalize ? (1.0f / 255.0f) : 1.0f;

        for (int row = 0; row < h; row++)
        {
            int row_offset_src = row * w * c;
            int row_offset_dst = row * w;
            for (int col = 0; col < w; col++)
            {
                int src_idx = row_offset_src + col * c;
                int dst_idx = row_offset_dst + col;

                float b = src[src_idx] * scale;
                float g = src[src_idx + 1] * scale;
                float r = src[src_idx + 2] * scale;

                dst[dst_idx] = r;           // 通道0 = R
                dst[hw + dst_idx] = g;      // 通道1 = G
                dst[hw + hw + dst_idx] = b; // 通道2 = B
            }
        }
    }

}