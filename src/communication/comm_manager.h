#pragma once

#include "modbus_client_base.h"
#include "light_serial_controller.h"
#include "app/common.h"
#include <QObject>
#include <QThread>
#include <QTimer>
#include <map>
#include <memory>
#include <mutex>
#include <array>

/// @brief 通信管理
///        管理 Modbus 通信、光源串口、IO 状态轮询、连接状态检测
class CommManager : public QObject
{
    Q_OBJECT
public:
    static CommManager &getInstance();

    /// @brief 停止子线程（必须在 QApplication 退出前调用）
    void shutdown();

    // IO 状态
    std::array<bool, 8> diState() const;
    std::array<bool, 8> doState() const;
    bool isCommConnected(const std::string &id) const;

    /// @brief 添加通信
    void addComm(const CommunicationParam &config);
    /// @brief 断开所有
    void disconnectAll();
    /// @brief 写单个线圈
    void writeSingleCoil(const std::string &comm_id, uint16_t addr, bool value);
    /// @brief 写多个寄存器
    void writeMultipleRegisters(const std::string &comm_id, uint16_t addr, const std::vector<uint16_t> &values);
    /// @brief 打开光源串口
    void openLightSerial(const std::string &port, int baud_rate);
    /// @brief 设置光源亮度
    void setLuminance(int channel, int luminance);

signals:
    void sign_ioStateUpdated();
    void sign_commStatusChanged(const std::string &id, bool connected);
    void sign_lightStatusChanged(bool connected);

private slots:
    void slot_refreshIO();
    void slot_checkConnection();

private:
    CommManager();
    ~CommManager();

    QThread worker_thread_;
    QTimer *io_timer_ = nullptr;
    QTimer *conn_timer_ = nullptr;

    std::map<std::string, std::unique_ptr<ModbusClientBase>> comms_;
    std::map<std::string, CommunicationParam> comm_configs_;  // 保存配置，用于重连
    LightSerialController *light_ctrl_ = nullptr;
    std::string light_port_;    // 光源串口名，用于重连
    int light_baud_rate_ = 38400;

    std::array<bool, 8> di_state_{};
    std::array<bool, 8> do_state_{};
    mutable std::mutex state_mutex_;
    mutable std::mutex comm_mutex_;
};
