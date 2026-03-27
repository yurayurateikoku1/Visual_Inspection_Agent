#include "inspection_pipeline.h"
#include "nodes/capture_node.h"
#include "nodes/algorithm_node.h"
#include "nodes/result_node.h"
#include "algorithm/algorithm_factory.h"
#include <spdlog/spdlog.h>

InspectionPipeline::InspectionPipeline(const WorkflowParam &param)
    : param_(param) {}

InspectionPipeline::~InspectionPipeline() = default;

void InspectionPipeline::build()
{
    taskflow_.clear();
    process_nodes_.clear();

    // 采集节点（单独管理，不放入 DAG）
    capture_node_ = std::make_unique<CaptureNode>(param_.camera_name);

    // 算法节点
    for (auto &algo_id : param_.algorithm_ids)
    {
        auto algo = AlgorithmFactory::instance().create(algo_id);
        if (algo)
        {
            process_nodes_.push_back(std::make_unique<AlgorithmNode>(std::move(algo)));
        }
        else
        {
            spdlog::warn("Algorithm {} not found, skipping", algo_id);
        }
    }

    // 结果节点
    process_nodes_.push_back(std::make_unique<ResultNode>(param_.comm_name));

    // 构建 Taskflow DAG：算法1 -> 算法2 -> ... -> 结果
    tf::Task prev;
    for (size_t i = 0; i < process_nodes_.size(); ++i)
    {
        auto *node = process_nodes_[i].get();
        auto task = taskflow_.emplace([node, this]()
                                      {
            if (!node->execute(ctx_)) {
                spdlog::error("Node {} failed", node->name());
            } })
                        .name(node->name());

        if (i > 0)
        {
            prev.precede(task);
        }
        prev = task;
    }

    spdlog::info("Pipeline {} built: 1 capture + {} process nodes",
                 param_.name, process_nodes_.size());
}

void InspectionPipeline::setOfflineImage(const HalconCpp::HObject &image)
{
    offline_image_ = image;
}

void InspectionPipeline::clearOfflineImage()
{
    offline_image_.Clear();
}

bool InspectionPipeline::capture()
{
    ctx_ = NodeContext{};
    ctx_.camera_name = param_.camera_name;
    ctx_.result.pass = true;

    // 离线模式：直接使用注入的图像
    if (offline_image_.IsInitialized())
    {
        ctx_.image = offline_image_;
        ctx_.display_image = offline_image_;
        return true;
    }

    // 在线模式：从相机采集
    if (!capture_node_ || !capture_node_->execute(ctx_))
    {
        spdlog::error("Pipeline {}: capture failed", param_.name);
        return false;
    }
    return true;
}

InspectionResult InspectionPipeline::process(tf::Executor &executor)
{
    executor.run(taskflow_).wait();
    return ctx_.result;
}
