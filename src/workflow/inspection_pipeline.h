#pragma once

#include "nodes/node_base.h"
#include "app/common.h"
#include <taskflow/taskflow.hpp>
#include <vector>
#include <memory>
#include <atomic>

class InspectionPipeline
{
public:
    explicit InspectionPipeline(const WorkflowConfig &config);
    ~InspectionPipeline();

    void build();
    void runOnce();
    void startLoop();
    void stop();
    bool isRunning() const { return running_; }

    const WorkflowConfig &config() const { return config_; }
    const NodeContext &lastContext() const { return ctx_; }

    // 动态添加算法节点
    void addAlgorithmNode(std::shared_ptr<IAlgorithm> algo);

private:
    WorkflowConfig config_;
    tf::Executor executor_;
    tf::Taskflow taskflow_;
    NodeContext ctx_;
    std::atomic<bool> running_{false};

    std::vector<std::unique_ptr<INode>> nodes_;
};
