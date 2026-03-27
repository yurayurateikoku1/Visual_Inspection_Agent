#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include "ai_common.h"
#include "ai_utils.h"

namespace AIInfer
{
    class PostProcessor
    {
    public:
        virtual ~PostProcessor() = default;
        virtual DetectionResult process(const TensorData &tensor,
                                        const LetterBoxInfo &lb_info,
                                        float score_thresh, float nms_thresh) = 0;

    protected:
        // 按 cls==0 为锚框进行分组，其他类别按中心点归属分配
        static std::vector<ObjectGroup> groupDetections(const std::vector<Detection> &dets);
        static std::vector<ObjectGroup> groupDetections(const std::vector<DetectionObb> &dets);
    };

    // 正框 NonMS: 张量格式 [batch, 4+N, anchors]
    class DetNonMSProcessor : public PostProcessor
    {
    public:
        DetectionResult process(const TensorData &tensor, const LetterBoxInfo &lb_info,
                                float score_thresh, float nms_thresh) override;
    };

    // 正框 End2End: 张量格式 [batch, 300, 6] (x1,y1,x2,y2,conf,cls)
    class DetEnd2EndProcessor : public PostProcessor
    {
    public:
        DetectionResult process(const TensorData &tensor, const LetterBoxInfo &lb_info,
                                float score_thresh, float nms_thresh) override;
    };

    // 旋转框 NonMS: 张量格式 [batch, 4+N+1, anchors]
    class ObbNonMSProcessor : public PostProcessor
    {
    public:
        DetectionResult process(const TensorData &tensor, const LetterBoxInfo &lb_info,
                                float score_thresh, float nms_thresh) override;
    };

    // 旋转框 End2End: 张量格式 [batch, 300, 7] (cx,cy,w,h,conf,cls,angle)
    class ObbEnd2EndProcessor : public PostProcessor
    {
    public:
        DetectionResult process(const TensorData &tensor, const LetterBoxInfo &lb_info,
                                float score_thresh, float nms_thresh) override;
    };

    // 判断输出是否为 End2End 格式
    bool isEnd2EndOutput(const std::vector<int> &shape, TaskType task);

    // 工厂：根据任务类型和是否 End2End 创建后处理器
    std::unique_ptr<PostProcessor> createPostProcessor(TaskType task, bool is_end2end);
}
