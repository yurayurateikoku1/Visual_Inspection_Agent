#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include "ai_common.h"
#include "infer_backend.h"
#include "ai_utils.h"
namespace AIInfer
{
    class PostProcessor
    {
    public:
        virtual ~PostProcessor() = default;
        virtual DetectionResult process(const std::vector<TensorData> &outputs,
                                        const LetterBoxInfo &lb_info,
                                        float score_thresh, float nms_thresh) = 0;
    };

    std::unique_ptr<PostProcessor> createPostProcessor(TaskType task, EngineType device);
}