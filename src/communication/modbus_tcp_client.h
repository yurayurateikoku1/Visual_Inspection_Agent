#pragma once

#include "modbus_client_base.h"
#include <QModbusTcpClient>

/// @brief Modbus TCP 客户端（网口 → PLC）
///        config.protocol = "modbus_tcp" 时由 CommManager 创建
class ModbusTcpClient : public ModbusClientBase
{
    Q_OBJECT
public:
    explicit ModbusTcpClient(const std::string &id, QObject *parent = nullptr);

    bool connectDevice(const CommunicationParam &config) override;
};
