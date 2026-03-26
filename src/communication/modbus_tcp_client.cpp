#include "modbus_tcp_client.h"
#include <QVariant>
#include <spdlog/spdlog.h>

ModbusTcpClient::ModbusTcpClient(const std::string &id, QObject *parent)
    : ModbusClientBase(id, parent)
{
    client_ = new QModbusTcpClient(this);
    client_->setTimeout(timeout_ms_);
    client_->setNumberOfRetries(3);
}

bool ModbusTcpClient::connectDevice(const CommunicationParam &config)
{
    if (isConnected())
    {
        spdlog::warn("Modbus TCP {} already connected", id_);
        return true;
    }

    server_address_ = config.slave_address;

    client_->setConnectionParameter(
        QModbusDevice::NetworkPortParameter, QVariant(config.port));
    client_->setConnectionParameter(
        QModbusDevice::NetworkAddressParameter, QVariant(QString::fromStdString(config.ip)));

    if (!client_->connectDevice())
    {
        spdlog::error("Modbus TCP connect failed: {} {}:{} - {}",
                      id_, config.ip, config.port, client_->errorString().toStdString());
        return false;
    }

    spdlog::info("Modbus TCP connecting: {} {}:{}...", id_, config.ip, config.port);
    return true;
}
