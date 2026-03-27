#include "postprocess.h"
#include <opencv2/opencv.hpp>
#include <numeric>

namespace AIInfer
{

    // 正框分组：cls==0 作为锚框，其他按中心点归属
    std::vector<ObjectGroup> PostProcessor::groupDetections(const std::vector<Detection> &dets)
    {
        std::vector<const Detection *> anchors;
        std::vector<const Detection *> others;
        for (const auto &d : dets)
        {
            if (d.cls == 0)
                anchors.push_back(&d);
            else
                others.push_back(&d);
        }

        // 每个 cls==0 框创建一个分组
        std::vector<ObjectGroup> groups(anchors.size());
        for (size_t i = 0; i < anchors.size(); i++)
        {
            groups[i].id = static_cast<int>(i);
            groups[i].dets = std::vector<Detection>{*anchors[i]};
        }

        // 其他框按中心点落入哪个锚框来归组
        for (const auto *det : others)
        {
            float cx = det->box.x + det->box.width / 2.0f;
            float cy = det->box.y + det->box.height / 2.0f;

            for (auto &group : groups)
            {
                const auto &anchor = std::get<std::vector<Detection>>(group.dets)[0];
                if (cx >= anchor.box.x && cx <= anchor.box.x + anchor.box.width &&
                    cy >= anchor.box.y && cy <= anchor.box.y + anchor.box.height)
                {
                    std::get<std::vector<Detection>>(group.dets).push_back(*det);
                    break;
                }
            }
        }

        // 按锚框左边 X 从左到右排序，重新编号
        std::sort(groups.begin(), groups.end(), [](const ObjectGroup &a, const ObjectGroup &b)
                  {
                  float ax = std::get<std::vector<Detection>>(a.dets)[0].box.x;
                  float bx = std::get<std::vector<Detection>>(b.dets)[0].box.x;
                  return ax < bx; });
        for (size_t i = 0; i < groups.size(); i++)
            groups[i].id = static_cast<int>(i);

        return groups;
    }

    // 旋转框分组
    std::vector<ObjectGroup> PostProcessor::groupDetections(const std::vector<DetectionObb> &dets)
    {
        std::vector<const DetectionObb *> anchors;
        std::vector<const DetectionObb *> others;
        for (const auto &d : dets)
        {
            if (d.detection.cls == 0)
                anchors.push_back(&d);
            else
                others.push_back(&d);
        }

        std::vector<ObjectGroup> groups(anchors.size());
        for (size_t i = 0; i < anchors.size(); i++)
        {
            groups[i].id = static_cast<int>(i);
            groups[i].dets = std::vector<DetectionObb>{*anchors[i]};
        }

        for (const auto *det : others)
        {
            float cx = det->detection.box.x + det->detection.box.width / 2.0f;
            float cy = det->detection.box.y + det->detection.box.height / 2.0f;

            for (auto &group : groups)
            {
                const auto &anchor = std::get<std::vector<DetectionObb>>(group.dets)[0];
                // 旋转框用 PointPolygonTest 判断
                cv::RotatedRect rr(
                    cv::Point2f(anchor.detection.box.x + anchor.detection.box.width / 2.0f,
                                anchor.detection.box.y + anchor.detection.box.height / 2.0f),
                    cv::Size2f(static_cast<float>(anchor.detection.box.width),
                               static_cast<float>(anchor.detection.box.height)),
                    anchor.angle * 180.0f / static_cast<float>(CV_PI));

                cv::Point2f pts[4];
                rr.points(pts);
                std::vector<cv::Point> contour(4);
                for (int j = 0; j < 4; j++)
                    contour[j] = cv::Point(static_cast<int>(pts[j].x), static_cast<int>(pts[j].y));

                if (cv::pointPolygonTest(contour, cv::Point2f(cx, cy), false) >= 0)
                {
                    std::get<std::vector<DetectionObb>>(group.dets).push_back(*det);
                    break;
                }
            }
        }

        // 按锚框中心 X 从左到右排序
        std::sort(groups.begin(), groups.end(), [](const ObjectGroup &a, const ObjectGroup &b)
                  {
                  const auto &ad = std::get<std::vector<DetectionObb>>(a.dets)[0];
                  const auto &bd = std::get<std::vector<DetectionObb>>(b.dets)[0];
                  float ax = ad.detection.box.x;
                  float bx = bd.detection.box.x;
                  return ax < bx; });
        for (size_t i = 0; i < groups.size(); i++)
            groups[i].id = static_cast<int>(i);

        return groups;
    }

    // ────────────────────── DetNonMSProcessor ──────────────────────
    // 张量格式: [batch, 4+N, anchors]，4 = xywh（中心点格式），N = 类别数

    DetectionResult DetNonMSProcessor::process(const TensorData &tensor,
                                               const LetterBoxInfo &lb_info,
                                               float score_thresh, float nms_thresh)
    {
        int num_features = tensor.shape[1]; // 4(xywh) + N(类别)
        int num_anchors = tensor.shape[2];
        int num_classes = num_features - 4;
        const float *data = static_cast<const float *>(tensor.data);

        std::vector<Detection> detections;
        detections.reserve(256);

        for (int a = 0; a < num_anchors; a++)
        {
            // 找最高置信度的类别
            int best_cls = 0;
            float best_score = -1e9f;
            for (int c = 0; c < num_classes; c++)
            {
                float score = data[(4 + c) * num_anchors + a];
                if (score > best_score)
                {
                    best_score = score;
                    best_cls = c;
                }
            }
            if (best_score < score_thresh)
                continue;

            // xywh 中心点 → 左上角 Rect
            float cx = data[0 * num_anchors + a];
            float cy = data[1 * num_anchors + a];
            float w = data[2 * num_anchors + a];
            float h = data[3 * num_anchors + a];

            cv::Rect box(static_cast<int>(cx - w / 2), static_cast<int>(cy - h / 2),
                         static_cast<int>(w), static_cast<int>(h));
            scaleBoxToOriginal(box, lb_info);

            detections.push_back({box, best_score, best_cls});
        }

        // NMS
        std::vector<int> indices;
        NMSBoxes(detections, score_thresh, nms_thresh, indices);

        std::vector<Detection> kept;
        kept.reserve(indices.size());
        for (int idx : indices)
            kept.push_back(detections[idx]);

        return kept;
    }

    // ────────────────────── DetEnd2EndProcessor ──────────────────────
    // 张量格式: [batch, 300, 6]，6 = x1,y1,x2,y2,conf,cls

    DetectionResult DetEnd2EndProcessor::process(const TensorData &tensor,
                                                 const LetterBoxInfo &lb_info,
                                                 float score_thresh, float /*nms_thresh*/)
    {
        int num_det = tensor.shape[1];
        int attrs = tensor.shape[2]; // 6
        const float *data = static_cast<const float *>(tensor.data);

        std::vector<Detection> detections;
        detections.reserve(num_det);

        for (int i = 0; i < num_det; i++)
        {
            int offset = i * attrs;
            float x1 = data[offset + 0];
            float y1 = data[offset + 1];
            float x2 = data[offset + 2];
            float y2 = data[offset + 3];
            float conf = data[offset + 4];
            int cls_id = static_cast<int>(data[offset + 5]);

            if (conf < score_thresh)
                continue;

            cv::Rect box(static_cast<int>(x1), static_cast<int>(y1),
                         static_cast<int>(x2 - x1), static_cast<int>(y2 - y1));
            scaleBoxToOriginal(box, lb_info);

            detections.push_back({box, conf, cls_id});
        }

        return detections;
    }

    // ────────────────────── ObbNonMSProcessor ──────────────────────
    // 张量格式: [batch, 4+N+1, anchors]，布局 [x,y,w,h, cls0..clsN, angle]

    DetectionResult ObbNonMSProcessor::process(const TensorData &tensor,
                                               const LetterBoxInfo &lb_info,
                                               float score_thresh, float nms_thresh)
    {
        int num_features = tensor.shape[1]; // 4(xywh) + N(类别) + 1(angle)
        int num_anchors = tensor.shape[2];
        int num_classes = num_features - 5; // 去掉 xywh(4) 和 angle(1)
        const float *data = static_cast<const float *>(tensor.data);

        std::vector<DetectionObb> detections;
        detections.reserve(256);

        for (int a = 0; a < num_anchors; a++)
        {
            int best_cls = 0;
            float best_score = -1e9f;
            for (int c = 0; c < num_classes; c++)
            {
                float score = data[(4 + c) * num_anchors + a];
                if (score > best_score)
                {
                    best_score = score;
                    best_cls = c;
                }
            }
            if (best_score < score_thresh)
                continue;

            float cx = data[0 * num_anchors + a];
            float cy = data[1 * num_anchors + a];
            float w = data[2 * num_anchors + a];
            float h = data[3 * num_anchors + a];
            float angle = data[(num_features - 1) * num_anchors + a]; // 弧度

            // 用 Detection 的 box 存 xywh（中心点格式，供分组使用）
            cv::Rect box(static_cast<int>(cx - w / 2), static_cast<int>(cy - h / 2),
                         static_cast<int>(w), static_cast<int>(h));

            // 坐标还原到原图（对旋转框用 RotatedRect 方式缩放）
            cv::RotatedRect rr(cv::Point2f(cx, cy), cv::Size2f(w, h),
                               angle * 180.0f / static_cast<float>(CV_PI));
            scaleRotatedBoxToOriginal(rr, lb_info);

            // 还原后重新算 Rect
            box = cv::Rect(static_cast<int>(rr.center.x - rr.size.width / 2),
                           static_cast<int>(rr.center.y - rr.size.height / 2),
                           static_cast<int>(rr.size.width),
                           static_cast<int>(rr.size.height));

            detections.push_back({{box, best_score, best_cls}, angle});
        }

        // 旋转框 NMS
        std::vector<int> indices;
        NMSBoxesRotated(detections, score_thresh, nms_thresh, indices);

        std::vector<DetectionObb> kept;
        kept.reserve(indices.size());
        for (int idx : indices)
            kept.push_back(detections[idx]);

        return kept;
    }

    // ────────────────────── ObbEnd2EndProcessor ──────────────────────
    // 张量格式: [batch, 300, 7]，7 = cx,cy,w,h,conf,cls,angle

    DetectionResult ObbEnd2EndProcessor::process(const TensorData &tensor,
                                                 const LetterBoxInfo &lb_info,
                                                 float score_thresh, float /*nms_thresh*/)
    {
        int num_det = tensor.shape[1];
        int attrs = tensor.shape[2]; // 7
        const float *data = static_cast<const float *>(tensor.data);

        std::vector<DetectionObb> detections;
        detections.reserve(num_det);

        for (int i = 0; i < num_det; i++)
        {
            int offset = i * attrs;
            float cx = data[offset + 0];
            float cy = data[offset + 1];
            float w = data[offset + 2];
            float h = data[offset + 3];
            float conf = data[offset + 4];
            int cls_id = static_cast<int>(data[offset + 5]);
            float angle = data[offset + 6]; // 弧度

            if (conf < score_thresh)
                continue;

            cv::RotatedRect rr(cv::Point2f(cx, cy), cv::Size2f(w, h),
                               angle * 180.0f / static_cast<float>(CV_PI));
            scaleRotatedBoxToOriginal(rr, lb_info);

            cv::Rect box(static_cast<int>(rr.center.x - rr.size.width / 2),
                         static_cast<int>(rr.center.y - rr.size.height / 2),
                         static_cast<int>(rr.size.width),
                         static_cast<int>(rr.size.height));

            detections.push_back({{box, conf, cls_id}, angle});
        }

        return detections;
    }

    bool isEnd2EndOutput(const std::vector<int> &shape, TaskType task)
    {
        if (shape.size() != 3)
            return false;
        int dim2 = shape[2];
        if (task == TaskType::YOLO_DET && dim2 == 6)
            return true;
        if (task == TaskType::YOLO_OBB && dim2 == 7)
            return true;
        return false;
    }

    std::unique_ptr<PostProcessor> createPostProcessor(TaskType task, bool is_end2end)
    {
        if (task == TaskType::YOLO_DET)
        {
            if (is_end2end)
                return std::make_unique<DetEnd2EndProcessor>();
            return std::make_unique<DetNonMSProcessor>();
        }
        if (is_end2end)
            return std::make_unique<ObbEnd2EndProcessor>();
        return std::make_unique<ObbNonMSProcessor>();
    }

} // namespace AIInfer
