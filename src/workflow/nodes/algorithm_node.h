#pragma once

#include "node_base.h"
#include "algorithm/algorithm_interface.h"
#include <memory>

class AlgorithmNode : public INode
{
public:
    explicit AlgorithmNode(std::shared_ptr<IAlgorithm> algo);
    std::string name() const override { return "Algorithm:" + algo_->name(); }
    bool execute(NodeContext &ctx) override;

private:
    std::shared_ptr<IAlgorithm> algo_;
};
