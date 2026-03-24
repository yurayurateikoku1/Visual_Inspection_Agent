#pragma once

#include "comm_interface.h"
#include "app/common.h"
#include <map>
#include <memory>
#include <mutex>

class CommManager
{
public:
    static CommManager &instance();

    bool addComm(const CommConfig &config);
    void removeComm(const std::string &id);
    IComm *getComm(const std::string &id);
    std::vector<std::string> commIds() const;

    void disconnectAll();

private:
    CommManager() = default;
    std::map<std::string, std::unique_ptr<IComm>> comms_;
    mutable std::mutex mutex_;
};
