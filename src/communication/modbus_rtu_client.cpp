#include "modbus_rtu_client.h"
#include <QVariant>
#include <spdlog/spdlog.h>

ModbusRtuClient::ModbusRtuClient(const std::string &name, QObject *parent)
    : ModbusClientBase(name, parent)
{
    client_ = new QModbusRtuSerialClient(this);
    client_->setTimeout(timeout_ms_);
    client_->setNumberOfRetries(3);
}

bool ModbusRtuClient::connectDevice(const CommunicationParam &config)
{
    if (isConnected())
    {
        spdlog::warn("Modbus RTU {} already connected", id_);
        return true;
    }

    server_address_ = config.slave_address;

    client_->setConnectionParameter(
        QModbusDevice::SerialPortNameParameter, QVariant(QString::fromStdString(config.serial_port)));
    client_->setConnectionParameter(
        QModbusDevice::SerialBaudRateParameter, QVariant(config.baud_rate));
    client_->setConnectionParameter(
        QModbusDevice::SerialDataBitsParameter, QVariant(config.data_bits));
    client_->setConnectionParameter(
        QModbusDevice::SerialStopBitsParameter, QVariant(config.stop_bits));
    client_->setConnectionParameter(
        QModbusDevice::SerialParityParameter, QVariant(config.parity));

    if (!client_->connectDevice())
    {
        spdlog::error("Modbus RTU connect failed: {} {} - {}",
                      id_, config.serial_port, client_->errorString().toStdString());
        return false;
    }

    spdlog::info("Modbus RTU connecting: {} {} @{}...", id_, config.serial_port, config.baud_rate);
    return true;
}
