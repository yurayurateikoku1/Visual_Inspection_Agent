#include "light_serial_controller.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

// 协议常量
static constexpr char STX = '$';               // 起始字符
static constexpr char CMD_OPEN = '1';          // 打开对应通道
static constexpr char CMD_CLOSE = '2';         // 关闭对应通道
static constexpr char CMD_SET_LUMINANCE = '3'; // 设置亮度

LightSerialController::LightSerialController(QObject *parent)
    : QObject(parent)
{
    serial_ = new QSerialPort(this);
}

LightSerialController::~LightSerialController()
{
    close();
}

bool LightSerialController::open(const std::string &port_name, int baud_rate)
{
    if (serial_->isOpen())
    {
        spdlog::warn("Light serial already open on {}", port_name);
        return true;
    }

    serial_->setPortName(QString::fromStdString(port_name));
    serial_->setBaudRate(baud_rate);
    serial_->setDataBits(QSerialPort::Data8);
    serial_->setStopBits(QSerialPort::OneStop);
    serial_->setParity(QSerialPort::NoParity);

    if (!serial_->open(QIODevice::ReadWrite))
    {
        spdlog::error("Light serial open failed: {} - {}",
                      port_name, serial_->errorString().toStdString());
        return false;
    }

    spdlog::info("Light serial opened: {} @{}", port_name, baud_rate);
    return true;
}

void LightSerialController::close()
{
    if (serial_ && serial_->isOpen())
    {
        serial_->close();
    }
}

bool LightSerialController::isOpen() const
{
    return serial_ && serial_->isOpen();
}

int LightSerialController::xorChecksum(const std::string &cmd)
{
    int xor_val = 0;
    for (char c : cmd)
    {
        xor_val ^= static_cast<unsigned char>(c);
    }
    return xor_val;
}

void LightSerialController::openChannel(int channel)
{
    // 格式: $1<ch>000<xor>
    std::string cmd;
    cmd += STX;
    cmd += CMD_OPEN;
    cmd += std::to_string(channel);
    cmd += "000";

    std::ostringstream oss;
    oss << std::uppercase << std::hex << xorChecksum(cmd);
    cmd += oss.str();

    sendMessage(cmd);
    spdlog::info("Light CH{} opened", channel);
}

void LightSerialController::closeChannel(int channel)
{
    // 格式: $2<ch>000<xor>
    std::string cmd;
    cmd += STX;
    cmd += CMD_CLOSE;
    cmd += std::to_string(channel);
    cmd += "000";

    std::ostringstream oss;
    oss << std::uppercase << std::hex << xorChecksum(cmd);
    cmd += oss.str();

    sendMessage(cmd);
    spdlog::info("Light CH{} closed", channel);
}

void LightSerialController::setLuminance(int channel, int luminance)
{
    // 格式: $3<ch>0<val_hex><xor>
    std::string cmd;
    cmd += STX;
    cmd += CMD_SET_LUMINANCE;
    cmd += std::to_string(channel);
    cmd += "0";

    std::ostringstream val_oss;
    val_oss << std::uppercase << std::hex << luminance;
    cmd += val_oss.str();

    std::ostringstream xor_oss;
    xor_oss << std::uppercase << std::hex << xorChecksum(cmd);
    cmd += xor_oss.str();

    sendMessage(cmd);
    spdlog::info("Light CH{} luminance set to {}", channel, luminance);
}

void LightSerialController::writePlc(int value)
{
    // 格式: 0x02 + value + 0x03
    QByteArray data;
    data.append(0x02);
    data.append(static_cast<char>(value));
    data.append(0x03);
    sendHex(data);
}

void LightSerialController::sendMessage(const std::string &msg)
{
    if (!isOpen())
    {
        spdlog::warn("Light serial not open, cannot send");
        return;
    }
    serial_->write(msg.c_str(), static_cast<qint64>(msg.size()));
}

void LightSerialController::sendHex(const QByteArray &data)
{
    if (!isOpen())
    {
        spdlog::warn("Light serial not open, cannot send hex");
        return;
    }
    serial_->write(data);
}
