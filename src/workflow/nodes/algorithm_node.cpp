#include "algorithm_node.h"
#include <spdlog/spdlog.h>

AlgorithmNode::AlgorithmNode(std::shared_ptr<IAlgorithm> algo)
    : algo_(std::move(algo)) {}

bool AlgorithmNode::execute(NodeContext &ctx)
{
    if (!algo_)
    {
        spdlog::error("AlgorithmNode: null algorithm");
        return false;
    }

    InspectionResult result;
    bool ok = algo_->process(ctx.image, result);
    if (!ok)
    {
        spdlog::error("AlgorithmNode: {} failed", algo_->name());
        return false;
    }

    // 合并结果 —— 任意一个算法 NG 则整体 NG
    if (!result.pass)
    {
        ctx.result.pass = false;
    }
    ctx.result.defect_regions.insert(ctx.result.defect_regions.end(),
                                     result.defect_regions.begin(),
                                     result.defect_regions.end());
    ctx.result.detail += result.detail + "; ";
    return true;
}
