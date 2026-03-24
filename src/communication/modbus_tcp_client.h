#pragma once

#include "comm_interface.h"
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QEventLoop>
#include <mutex>
#include <atomic>

class ModbusTcpClient : public IComm
{
public:
    explicit ModbusTcpClient(const std::string &id);
    ~ModbusTcpClient() override;

    bool connect(const std::string &ip, int port) override;
    void disconnect() override;
    bool isConnected() const override;

    bool readHoldingRegisters(uint16_t addr, uint16_t count, std::vector<uint16_t> &values) override;
    bool writeSingleRegister(uint16_t addr, uint16_t value) override;
    bool writeMultipleRegisters(uint16_t addr, const std::vector<uint16_t> &values) override;

    bool readCoils(uint16_t addr, uint16_t count, std::vector<bool> &values) override;
    bool writeSingleCoil(uint16_t addr, bool value) override;

    bool sendResult(uint16_t addr, bool pass) override;

    std::string id() const override { return id_; }

    void setServerAddress(int address) { server_address_ = address; }
    void setTimeout(int ms) { timeout_ms_ = ms; }

private:
    bool sendWriteRequest(const QModbusDataUnit &unit);
    std::optional<QModbusDataUnit> sendReadRequest(const QModbusDataUnit &unit);

    std::string id_;
    QModbusTcpClient *client_ = nullptr;
    int server_address_ = 1;
    int timeout_ms_ = 1000;
    std::mutex mutex_;
};
