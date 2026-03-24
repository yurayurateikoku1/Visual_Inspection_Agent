#pragma once

#include <string>
#include <cstdint>
#include <vector>

class IComm
{
public:
    virtual ~IComm() = default;

    virtual bool connect(const std::string &ip, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Modbus 寄存器读写
    virtual bool readHoldingRegisters(uint16_t addr, uint16_t count, std::vector<uint16_t> &values) = 0;
    virtual bool writeSingleRegister(uint16_t addr, uint16_t value) = 0;
    virtual bool writeMultipleRegisters(uint16_t addr, const std::vector<uint16_t> &values) = 0;

    // Modbus 线圈读写
    virtual bool readCoils(uint16_t addr, uint16_t count, std::vector<bool> &values) = 0;
    virtual bool writeSingleCoil(uint16_t addr, bool value) = 0;

    // 便捷方法：发送检测结果
    virtual bool sendResult(uint16_t addr, bool pass) = 0;

    virtual std::string id() const = 0;
};
