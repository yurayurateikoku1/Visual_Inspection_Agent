#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <map>
#include <memory>
#include <mutex>

/// @brief 相机管理
class CameraManager
{
public:
    static CameraManager &getInstance();

    /// @brief 添加相机（按 IP 枚举匹配）
    bool addCamera(const CameraConfig &config);

    /// @brief 添加相机（直接传入设备信息，免二次枚举）
    bool addCamera(const CameraConfig &config, MV_CC_DEVICE_INFO *dev_info);

    void removeCamera(const std::string &id);
    ICamera *getCamera(const std::string &id);
    std::vector<std::string> cameraIds() const;

    void openAll();
    void closeAll();

private:
    CameraManager() = default;
    std::map<std::string, std::unique_ptr<ICamera>> cameras_;
    mutable std::mutex mutex_;
};
