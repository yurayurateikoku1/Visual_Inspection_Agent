#include "comm_manager.h"
#include "modbus_tcp_client.h"
#include <spdlog/spdlog.h>

CommManager &CommManager::instance()
{
    static CommManager inst;
    return inst;
}

bool CommManager::addComm(const CommConfig &config)
{
    std::lock_guard lock(mutex_);
    if (comms_.count(config.id))
    {
        spdlog::warn("Comm {} already exists", config.id);
        return false;
    }

    std::unique_ptr<IComm> comm;
    if (config.protocol == "modbus_tcp")
    {
        comm = std::make_unique<ModbusTcpClient>(config.id);
    }
    else
    {
        spdlog::error("Unknown protocol: {}", config.protocol);
        return false;
    }

    if (!comm->connect(config.ip, config.port))
    {
        return false;
    }
    comms_[config.id] = std::move(comm);
    return true;
}

void CommManager::removeComm(const std::string &id)
{
    std::lock_guard lock(mutex_);
    if (auto it = comms_.find(id); it != comms_.end())
    {
        it->second->disconnect();
        comms_.erase(it);
    }
}

IComm *CommManager::getComm(const std::string &id)
{
    std::lock_guard lock(mutex_);
    auto it = comms_.find(id);
    return it != comms_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> CommManager::commIds() const
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> ids;
    for (auto &[id, _] : comms_)
        ids.push_back(id);
    return ids;
}

void CommManager::disconnectAll()
{
    std::lock_guard lock(mutex_);
    for (auto &[id, comm] : comms_)
        comm->disconnect();
    comms_.clear();
}
