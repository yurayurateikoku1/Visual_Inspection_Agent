#pragma once

#include "modbus_client_base.h"
#include <QModbusRtuSerialClient>

/// @brief Modbus RTU 客户端（串口 → PLC/外设）
///        config.protocol = "modbus_rtu" 时由 CommManager 创建
class ModbusRtuClient : public ModbusClientBase
{
    Q_OBJECT
public:
    explicit ModbusRtuClient(const std::string &name, QObject *parent = nullptr);

    bool connectDevice(const CommunicationParam &config) override;
};
