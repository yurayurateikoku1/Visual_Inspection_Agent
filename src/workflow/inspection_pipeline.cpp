#include "inspection_pipeline.h"
#include "nodes/capture_node.h"
#include "nodes/algorithm_node.h"
#include "nodes/result_node.h"
#include "algorithm/algorithm_factory.h"
#include <spdlog/spdlog.h>
#include <thread>

InspectionPipeline::InspectionPipeline(const WorkflowConfig &config)
    : config_(config), executor_(1) {}

InspectionPipeline::~InspectionPipeline()
{
    stop();
}

void InspectionPipeline::addAlgorithmNode(std::shared_ptr<IAlgorithm> algo)
{
    config_.algorithm_ids.push_back(algo->name());
    nodes_.push_back(std::make_unique<AlgorithmNode>(std::move(algo)));
}

void InspectionPipeline::build()
{
    taskflow_.clear();
    nodes_.clear();

    // 1. 采集节点
    auto capture = std::make_unique<CaptureNode>(config_.camera_id);
    nodes_.push_back(std::move(capture));

    // 2. 算法节点
    for (auto &algo_id : config_.algorithm_ids)
    {
        auto algo = AlgorithmFactory::instance().create(algo_id);
        if (algo)
        {
            nodes_.push_back(std::make_unique<AlgorithmNode>(std::move(algo)));
        }
        else
        {
            spdlog::warn("Algorithm {} not found, skipping", algo_id);
        }
    }

    // 3. 结果节点
    nodes_.push_back(std::make_unique<ResultNode>(config_.comm_id));

    // 构建 Taskflow DAG：采集 -> 算法1 -> 算法2 -> ... -> 结果
    tf::Task prev;
    for (size_t i = 0; i < nodes_.size(); ++i)
    {
        auto *node = nodes_[i].get();
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

    spdlog::info("Pipeline {} built with {} nodes", config_.name, nodes_.size());
}

void InspectionPipeline::runOnce()
{
    ctx_ = NodeContext{};
    ctx_.result.pass = true;
    executor_.run(taskflow_).wait();
}

void InspectionPipeline::startLoop()
{
    if (running_)
        return;
    running_ = true;

    std::thread([this]()
                {
        spdlog::info("Pipeline {} loop started", config_.name);
        while (running_) {
            runOnce();
        }
        spdlog::info("Pipeline {} loop stopped", config_.name); })
        .detach();
}

void InspectionPipeline::stop()
{
    running_ = false;
    executor_.wait_for_all();
}
