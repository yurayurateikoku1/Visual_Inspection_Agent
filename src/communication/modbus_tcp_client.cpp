#include "modbus_tcp_client.h"
#include <spdlog/spdlog.h>
#include <QTimer>

ModbusTcpClient::ModbusTcpClient(const std::string &id) : id_(id)
{
    client_ = new QModbusTcpClient();
    client_->setTimeout(timeout_ms_);
    client_->setNumberOfRetries(3);
}

ModbusTcpClient::~ModbusTcpClient()
{
    disconnect();
    delete client_;
}

bool ModbusTcpClient::connect(const std::string &ip, int port)
{
    std::lock_guard lock(mutex_);

    if (client_->state() == QModbusDevice::ConnectedState)
    {
        spdlog::warn("Modbus {} already connected", id_);
        return true;
    }

    client_->setConnectionParameter(
        QModbusDevice::NetworkPortParameter, port);
    client_->setConnectionParameter(
        QModbusDevice::NetworkAddressParameter, QString::fromStdString(ip));

    if (!client_->connectDevice())
    {
        spdlog::error("Modbus connect failed: {} {}:{} - {}",
                      id_, ip, port, client_->errorString().toStdString());
        return false;
    }

    // 等待连接建立
    QEventLoop loop;
    QObject::connect(client_, &QModbusClient::stateChanged, &loop,
                     [&loop](QModbusDevice::State state)
                     {
                         if (state == QModbusDevice::ConnectedState ||
                             state == QModbusDevice::UnconnectedState)
                         {
                             loop.quit();
                         }
                     });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    if (client_->state() != QModbusDevice::ConnectedState)
    {
        spdlog::error("Modbus connect timeout: {} {}:{}", id_, ip, port);
        return false;
    }

    spdlog::info("Modbus connected: {} {}:{}", id_, ip, port);
    return true;
}

void ModbusTcpClient::disconnect()
{
    std::lock_guard lock(mutex_);
    if (client_->state() != QModbusDevice::UnconnectedState)
    {
        client_->disconnectDevice();
    }
}

bool ModbusTcpClient::isConnected() const
{
    return client_->state() == QModbusDevice::ConnectedState;
}

std::optional<QModbusDataUnit> ModbusTcpClient::sendReadRequest(const QModbusDataUnit &unit)
{
    auto *reply = client_->sendReadRequest(unit, server_address_);
    if (!reply)
    {
        spdlog::error("Modbus read request failed: {}", id_);
        return std::nullopt;
    }

    // 同步等待应答
    QEventLoop loop;
    QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeout_ms_, &loop, &QEventLoop::quit);
    loop.exec();

    std::optional<QModbusDataUnit> result;
    if (reply->isFinished() && reply->error() == QModbusDevice::NoError)
    {
        result = reply->result();
    }
    else
    {
        spdlog::error("Modbus read error: {} - {}", id_, reply->errorString().toStdString());
    }

    reply->deleteLater();
    return result;
}

bool ModbusTcpClient::sendWriteRequest(const QModbusDataUnit &unit)
{
    auto *reply = client_->sendWriteRequest(unit, server_address_);
    if (!reply)
    {
        spdlog::error("Modbus write request failed: {}", id_);
        return false;
    }

    QEventLoop loop;
    QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeout_ms_, &loop, &QEventLoop::quit);
    loop.exec();

    bool ok = reply->isFinished() && reply->error() == QModbusDevice::NoError;
    if (!ok)
    {
        spdlog::error("Modbus write error: {} - {}", id_, reply->errorString().toStdString());
    }

    reply->deleteLater();
    return ok;
}

bool ModbusTcpClient::readHoldingRegisters(uint16_t addr, uint16_t count, std::vector<uint16_t> &values)
{
    std::lock_guard lock(mutex_);
    if (!isConnected())
        return false;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, count);
    auto result = sendReadRequest(unit);
    if (!result)
        return false;

    values.resize(result->valueCount());
    for (uint i = 0; i < result->valueCount(); ++i)
    {
        values[i] = result->value(i);
    }
    return true;
}

bool ModbusTcpClient::writeSingleRegister(uint16_t addr, uint16_t value)
{
    std::lock_guard lock(mutex_);
    if (!isConnected())
        return false;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, 1);
    unit.setValue(0, value);
    return sendWriteRequest(unit);
}

bool ModbusTcpClient::writeMultipleRegisters(uint16_t addr, const std::vector<uint16_t> &values)
{
    std::lock_guard lock(mutex_);
    if (!isConnected())
        return false;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, static_cast<uint16_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i)
    {
        unit.setValue(static_cast<int>(i), values[i]);
    }
    return sendWriteRequest(unit);
}

bool ModbusTcpClient::readCoils(uint16_t addr, uint16_t count, std::vector<bool> &values)
{
    std::lock_guard lock(mutex_);
    if (!isConnected())
        return false;

    QModbusDataUnit unit(QModbusDataUnit::Coils, addr, count);
    auto result = sendReadRequest(unit);
    if (!result)
        return false;

    values.resize(result->valueCount());
    for (uint i = 0; i < result->valueCount(); ++i)
    {
        values[i] = result->value(i) != 0;
    }
    return true;
}

bool ModbusTcpClient::writeSingleCoil(uint16_t addr, bool value)
{
    std::lock_guard lock(mutex_);
    if (!isConnected())
        return false;

    QModbusDataUnit unit(QModbusDataUnit::Coils, addr, 1);
    unit.setValue(0, value ? 1 : 0);
    return sendWriteRequest(unit);
}

bool ModbusTcpClient::sendResult(uint16_t addr, bool pass)
{
    return writeSingleRegister(addr, pass ? 1 : 0);
}
