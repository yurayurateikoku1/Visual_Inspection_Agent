#include "result_node.h"
#include <spdlog/spdlog.h>
#include <chrono>

ResultNode::ResultNode(const std::string &comm_name, uint16_t result_addr)
    : comm_name_(comm_name), result_addr_(result_addr) {}

bool ResultNode::execute(NodeContext &ctx)
{
    auto now = std::chrono::system_clock::now();
    ctx.result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch())
                                  .count();

    // DO 输出由 WorkflowManager 统一处理，这里只做日志和存储

    // TODO: 保存到数据库（DatabaseManager 未实现）
    // DatabaseManager::instance().saveResult(ctx.camera_name, ctx.result);

    spdlog::info("ResultNode: camera={} pass={} detail={}", ctx.camera_name, ctx.result.pass, ctx.result.detail);
    return true;
}
