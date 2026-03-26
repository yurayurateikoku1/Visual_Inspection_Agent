#pragma once

#include <QObject>
#include <QSerialPort>
#include <string>

/// @brief 光源串口控制器
///        自定义 ASCII 协议，波特率 38400
///        协议格式: "$" + 命令码 + 通道号 + 数据 + XOR校验
///        命令码: 1=开, 2=关, 3=设置亮度, 4=读取亮度, 7=触发频闪
class LightSerialController : public QObject
{
    Q_OBJECT
public:
    explicit LightSerialController(QObject *parent = nullptr);
    ~LightSerialController();

    /// @brief 打开串口
    bool open(const std::string &port_name, int baud_rate = 38400);
    /// @brief 关闭串口
    void close();
    bool isOpen() const;

    /// @brief 打开光源通道 (1~4)
    void openChannel(int channel);
    /// @brief 关闭光源通道 (1~4)
    void closeChannel(int channel);
    /// @brief 设置通道亮度 (channel: 1~4, luminance: 0~255)
    void setLuminance(int channel, int luminance);

    /// @brief 发送简单 PLC 指令: 0x02 + value + 0x03
    void writePlc(int value);

private:
    /// @brief 计算 XOR 校验
    static int xorChecksum(const std::string &cmd);
    /// @brief 发送 ASCII 字符串
    void sendMessage(const std::string &msg);
    /// @brief 发送 hex 字节
    void sendHex(const QByteArray &data);

    QSerialPort *serial_ = nullptr;
};
