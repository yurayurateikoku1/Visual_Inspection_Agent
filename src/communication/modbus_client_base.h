#pragma once

#include "../app/common.h"
#include <QObject>
#include <QModbusClient>
#include <QModbusDataUnit>
#include <string>
#include <cstdint>
#include <vector>
#include <functional>

/// @brief 读寄存器回调: (success, values)
using ReadRegistersCallback = std::function<void(bool, const std::vector<uint16_t> &)>;
/// @brief 读线圈回调: (success, values)
using ReadCoilsCallback = std::function<void(bool, const std::vector<bool> &)>;
/// @brief 写操作回调: (success)
using WriteCallback = std::function<void(bool)>;

/// @brief Modbus 客户端基类（异步模式）
///        提取 TCP/RTU 的公共逻辑：异步读写、断连、状态查询
///        子类只需实现 connect()
class ModbusClientBase : public QObject
{
    Q_OBJECT
public:
    explicit ModbusClientBase(const std::string &id, QObject *parent = nullptr);
    virtual ~ModbusClientBase();

    /// @brief 连接设备（子类实现 TCP/RTU 具体连接逻辑）
    virtual bool connectDevice(const CommunicationParam &config) = 0;
    /// @brief 断开连接
    void disconnectDevice();
    /// @brief 查询当前是否处于已连接状态
    bool isConnected() const;

    // ── 4X 保持寄存器操作 ──

    /// @brief 异步读保持寄存器 (FC03)，结果通过回调返回
    void readHoldingRegisters(uint16_t addr, uint16_t count, ReadRegistersCallback cb);
    /// @brief 异步写单个保持寄存器 (FC06)
    void writeSingleRegister(uint16_t addr, uint16_t value, WriteCallback cb = nullptr);
    /// @brief 异步写多个保持寄存器 (FC10)
    void writeMultipleRegisters(uint16_t addr, const std::vector<uint16_t> &values, WriteCallback cb = nullptr);

    // ── 0X 线圈操作 ──

    /// @brief 异步读线圈 (FC01)，结果通过回调返回
    void readCoils(uint16_t addr, uint16_t count, ReadCoilsCallback cb);
    /// @brief 异步写单个线圈 (FC05)
    void writeSingleCoil(uint16_t addr, bool value, WriteCallback cb = nullptr);

    /// @brief 写检测结果到线圈（pass=true 写 OK 地址，false 写 NG 地址）
    void sendResult(uint16_t addr, bool pass, WriteCallback cb = nullptr);

    std::string getId() const { return id_; }

    /// @brief 设置 Modbus 从站地址（默认 1）
    void setServerAddress(int address) { server_address_ = address; }
    /// @brief 设置请求超时时间（默认 1000ms）
    void setTimeout(int ms) { timeout_ms_ = ms; }

protected:
    QModbusClient *client_ = nullptr; // 子类提供具体的 QModbusClient 实例（TCP 或 RTU）
    std::string id_;                  // 通信通道标识，如 "plc_1"
    int server_address_ = 1;          // Modbus 从站地址
    int timeout_ms_ = 1000;           // 请求超时（ms）

private:
    /// @brief 异步发送读请求，应答通过回调返回
    void asyncRead(const QModbusDataUnit &unit, ReadRegistersCallback reg_cb, ReadCoilsCallback coil_cb);
    /// @brief 异步发送写请求，应答通过回调返回
    void asyncWrite(const QModbusDataUnit &unit, WriteCallback cb);
};
