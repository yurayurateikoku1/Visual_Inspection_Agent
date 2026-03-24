#pragma once

#include "node_base.h"

class CaptureNode : public INode
{
public:
    explicit CaptureNode(const std::string &camera_id);
    std::string name() const override { return "Capture"; }
    bool execute(NodeContext &ctx) override;

private:
    std::string camera_id_;
};
