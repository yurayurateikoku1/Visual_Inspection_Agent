#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <QObject>
#include <QTimer>
#include <map>
#include <set>
#include <memory>
#include <mutex>

/// @brief 相机管理
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

    /// @brief 将相机标记为离线并启动重连定时器
    void markOffline(const std::string &camera_name);

signals:
    /// @brief 相机状态变化 (online=true 上线, online=false 离线)
    void sign_cameraStatusChanged(const std::string &camera_name, bool online);

private slots:

    /// @brief 重连相机定时器
    void slot_reconnectTimer();

private:
    CameraManager();

    std::map<std::string, std::unique_ptr<ICamera>> cameras_; // name → 相机实例
    std::set<std::string> offline_cameras_;                   // 离线相机 name
    mutable std::mutex mutex_;
    QTimer *reconnect_timer_ = nullptr; // 重连定时器

    static constexpr int RECONNECT_INTERVAL_MS = 3000; // 重连间隔3s
};
