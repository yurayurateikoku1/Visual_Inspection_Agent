#pragma once

#include "node_base.h"
#include <string>

class ResultNode : public INode
{
public:
    explicit ResultNode(const std::string &comm_id, uint16_t result_addr = 100);
    std::string name() const override { return "Result"; }
    bool execute(NodeContext &ctx) override;

private:
    std::string comm_id_;
    uint16_t result_addr_;
};
