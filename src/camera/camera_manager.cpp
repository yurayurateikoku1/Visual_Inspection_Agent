#include "camera_manager.h"
#include "hik_camera.h"
#include "app/app_context.h"
#include <spdlog/spdlog.h>

CameraManager::CameraManager()
    : QObject(nullptr)
{
    reconnect_timer_ = new QTimer(this);
    reconnect_timer_->setInterval(RECONNECT_INTERVAL_MS);
    connect(reconnect_timer_, &QTimer::timeout, this, &CameraManager::slot_reconnectTimer);

    enumAndOpenAll();

    // 始终启动定时器：心跳检测在线相机 + 重新枚举发现新设备
    reconnect_timer_->start();
}

CameraManager &CameraManager::getInstance()
{
    static CameraManager inst;
    return inst;
}

void CameraManager::enumAndOpenAll()
{
    auto devices = HikCamera::enumDevices();
    if (devices.empty())
    {
        SPDLOG_WARN("No HIK camera devices found");
        return;
    }
    SPDLOG_INFO("Found {} HIK devices", devices.size());

    // 构建枚举设备的 name → (dev_info, ip) 映射
    struct DevEntry { MV_CC_DEVICE_INFO *info; std::string ip; };
    std::map<std::string, DevEntry> name_dev_map;
    for (auto &dev : devices)
    {
        if (dev.nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = dev.SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            std::string name = gige.chUserDefinedName[0]
                                   ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                                   : "";
            name_dev_map[name] = {&dev, ip};
            SPDLOG_INFO("Enumerated GigE device: Name={}, IP={}", name, ip);
        }
    }

    // 按配置中的相机参数，根据 name 匹配枚举到的设备
    for (auto &cfg : AppContext::getInstance().cameraParams())
    {
        auto it = name_dev_map.find(cfg.name);
        if (it == name_dev_map.end())
        {
            SPDLOG_WARN("Camera {} not found in enumeration", cfg.name);
            continue;
        }

        // 用枚举到的实际 IP 更新配置
        cfg.ip = it->second.ip;

        if (addCamera(cfg, it->second.info))
        {
            auto *cam = getCamera(cfg.name);
            cam->startGrabbing();
            SPDLOG_INFO("Camera {} (IP: {}) started", cfg.name, cfg.ip);
        }
    }
}

bool CameraManager::addCamera(const CameraParam &config)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.name))
    {
        SPDLOG_WARN("Camera {} already exists", config.name);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->open(config))
    {
        return false;
    }
    cameras_[config.name] = std::move(cam);
    return true;
}

bool CameraManager::addCamera(const CameraParam &config, MV_CC_DEVICE_INFO *dev_info)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.name))
    {
        SPDLOG_WARN("Camera {} already exists", config.name);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->open(config, dev_info))
    {
        return false;
    }
    cameras_[config.name] = std::move(cam);
    return true;
}

void CameraManager::removeCamera(const std::string &name)
{
    std::lock_guard lock(mutex_);
    if (auto it = cameras_.find(name); it != cameras_.end())
    {
        it->second->close();
        cameras_.erase(it);
    }
    offline_cameras_.erase(name);
}

ICamera *CameraManager::getCamera(const std::string &name)
{
    std::lock_guard lock(mutex_);
    auto it = cameras_.find(name);
    return it != cameras_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> CameraManager::cameraNames() const
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(cameras_.size());
    for (auto &[name, _] : cameras_)
    {
        names.push_back(name);
    }
    return names;
}

void CameraManager::openAll()
{
    std::lock_guard lock(mutex_);
    for (auto &[name, cam] : cameras_)
    {
        if (!cam->isOpened())
        {
            SPDLOG_WARN("Camera {} is not opened", name);
        }
    }
}

void CameraManager::closeAll()
{
    reconnect_timer_->stop();
    offline_cameras_.clear();

    std::lock_guard lock(mutex_);
    for (auto &[name, cam] : cameras_)
    {
        cam->close();
    }
    cameras_.clear();
}

void CameraManager::markOffline(const std::string &camera_name)
{
    {
        std::lock_guard lock(mutex_);
        auto it = cameras_.find(camera_name);
        if (it != cameras_.end())
        {
            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (hik_cam)
                hik_cam->forceClose();
        }
    }
    offline_cameras_.insert(camera_name);
    emit sign_cameraStatusChanged(camera_name, false);
    SPDLOG_INFO("Camera {} marked offline", camera_name);
}

void CameraManager::slot_reconnectTimer()
{
    // 只枚举一次，结果用于：心跳检测、离线重连、新设备发现
    auto devices = HikCamera::enumDevices();

    // 构建 name → (dev_info*, ip) 映射
    struct DevEntry { MV_CC_DEVICE_INFO *info; std::string ip; };
    std::map<std::string, DevEntry> online_devs;
    for (auto &dev : devices)
    {
        if (dev.nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = dev.SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            std::string name = gige.chUserDefinedName[0]
                                   ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                                   : "";
            online_devs[name] = {&dev, ip};
        }
    }

    // 收集状态变化，释放锁后再 emit
    std::vector<std::pair<std::string, bool>> status_changes;

    // ── 1. 心跳检测：在线相机的 name 不在枚举结果中 → 掉线 ──
    {
        std::lock_guard lock(mutex_);
        for (auto &[cam_name, cam] : cameras_)
        {
            if (offline_cameras_.count(cam_name))
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(cam.get());
            if (!hik_cam || !hik_cam->isOpened())
                continue;

            if (online_devs.find(hik_cam->config().name) == online_devs.end())
            {
                SPDLOG_WARN("Camera {} not found in enum, marking offline", cam_name);
                hik_cam->forceClose();
                offline_cameras_.insert(cam_name);
                status_changes.emplace_back(cam_name, false);
            }
        }
    }

    // ── 2. 尝试重连离线相机（name 重新出现在枚举结果中才尝试） ──
    if (!offline_cameras_.empty())
    {
        auto to_reconnect = offline_cameras_;
        for (const auto &cam_name : to_reconnect)
        {
            std::lock_guard lock(mutex_);
            auto it = cameras_.find(cam_name);
            if (it == cameras_.end())
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (!hik_cam)
                continue;

            if (online_devs.find(hik_cam->config().name) == online_devs.end())
                continue;

            if (hik_cam->reconnect())
            {
                offline_cameras_.erase(cam_name);
                status_changes.emplace_back(cam_name, true);
            }
            else
            {
                SPDLOG_WARN("Camera {} reconnect failed, will retry", cam_name);
            }
        }
    }

    // ── 3. 尝试打开配置中尚未管理的相机（启动时未连接，现在上线了） ──
    {
        for (auto &cfg : AppContext::getInstance().cameraParams())
        {
            {
                std::lock_guard lock(mutex_);
                if (cameras_.count(cfg.name))
                    continue;
            }
            if (offline_cameras_.count(cfg.name))
                continue;

            auto it = online_devs.find(cfg.name);
            if (it == online_devs.end())
                continue;

            // 用枚举到的实际 IP 更新配置
            cfg.ip = it->second.ip;

            if (addCamera(cfg, it->second.info))
            {
                auto *cam = getCamera(cfg.name);
                if (cam)
                {
                    cam->startGrabbing();
                    status_changes.emplace_back(cfg.name, true);
                    SPDLOG_INFO("Camera {} (IP: {}) connected", cfg.name, cfg.ip);
                }
            }
        }
    }

    // ── 4. 释放锁，安全发送信号 ──
    for (auto &[cam_name, online] : status_changes)
    {
        emit sign_cameraStatusChanged(cam_name, online);
    }
}
