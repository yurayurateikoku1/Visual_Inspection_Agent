#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <QObject>
#include <QTimer>
#include <map>
#include <set>
#include <memory>
#include <mutex>

/// @brief 相机在线检测状态机 + 事件驱动模型
///
/// 设备枚举（GVCP Discovery）仅在以下三种情况下执行：
///   1. 程序启动时
///   2. 心跳检测失败时（已打开相机的 Control Channel 读寄存器超时）
///   3. 外部请求设备发现时（热插拔/网卡变化等 OS 通知）
///
/// 心跳通过 Control Channel 定时读取相机只读寄存器实现，不使用枚举。
class CameraManager : public QObject
{
    Q_OBJECT
public:
    static CameraManager &getInstance();

    /// @brief 枚举所有海康设备并自动打开（构造时调用）
    void enumAndOpenAll();

    /// @brief 添加相机（按 IP 枚举匹配）
    bool addCamera(const CameraParam &config);

    /// @brief 添加相机（直接传入设备信息，免二次枚举）
    bool addCamera(const CameraParam &config, MV_CC_DEVICE_INFO *dev_info);

    void removeCamera(const std::string &name);
    ICamera *getCamera(const std::string &name);
    std::vector<std::string> cameraNames() const;

    void openAll();
    void closeAll();

    /// @brief 将相机标记为离线（触发枚举+重连流程）
    void markOffline(const std::string &camera_name);

    /// @brief 外部请求重新枚举设备（热插拔/网卡变化等 OS 通知调用）
    void requestDiscovery();

signals:
    /// @brief 相机状态变化 (online=true 上线, online=false 离线)
    void sign_cameraStatusChanged(const std::string &camera_name, bool online);

private slots:
    /// @brief 心跳定时器：通过 Control Channel 读寄存器检测在线相机
    void slot_heartbeatTimer();

    /// @brief 执行一次设备枚举（重连离线相机 + 发现新设备）
    void slot_doDiscovery();

private:
    CameraManager();

    std::map<std::string, std::unique_ptr<ICamera>> cameras_; // name → 相机实例
    std::set<std::string> offline_cameras_;                   // 离线相机 name
    mutable std::mutex mutex_;

    QTimer *heartbeat_timer_ = nullptr;   // 心跳定时器（周期性读寄存器）
    QTimer *discovery_timer_ = nullptr;   // 单次枚举定时器（事件触发，延迟去抖）

    static constexpr int HEARTBEAT_INTERVAL_MS = 3000;  // 心跳间隔 3s
    static constexpr int DISCOVERY_DEBOUNCE_MS = 500;   // 枚举去抖延迟 500ms
};
