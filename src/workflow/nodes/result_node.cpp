#include "result_node.h"
#include "communication/comm_manager.h"
#include "storage/database_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>

ResultNode::ResultNode(const std::string &comm_id, uint16_t result_addr)
    : comm_id_(comm_id), result_addr_(result_addr) {}

bool ResultNode::execute(NodeContext &ctx)
{
    auto now = std::chrono::system_clock::now();
    ctx.result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch())
                                  .count();

    // 发送结果到 PLC
    if (!comm_id_.empty())
    {
        auto *comm = CommManager::instance().getComm(comm_id_);
        if (comm && comm->isConnected())
        {
            comm->sendResult(result_addr_, ctx.result.pass);
        }
        else
        {
            spdlog::warn("ResultNode: comm {} not available", comm_id_);
        }
    }

    // 保存到数据库
    DatabaseManager::instance().saveResult(ctx.camera_id, ctx.result);

    spdlog::info("ResultNode: camera={} pass={} detail={}", ctx.camera_id, ctx.result.pass, ctx.result.detail);
    return true;
}
