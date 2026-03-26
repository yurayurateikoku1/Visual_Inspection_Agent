#include "camera_manager.h"
#include "hik_camera.h"
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

    for (size_t i = 0; i < devices.size() && i < 4; ++i)
    {
        auto &dev = devices[i];

        CameraParam cfg;
        cfg.id = "cam_" + std::to_string(i + 1);

        if (dev.nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = dev.SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            cfg.ip = ip;
            cfg.name = (gige.chUserDefinedName[0] != '\0')
                           ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                           : cfg.ip;
        }
        else if (dev.nTLayerType == MV_USB_DEVICE)
        {
            auto &usb = dev.SpecialInfo.stUsb3VInfo;
            cfg.name = (usb.chUserDefinedName[0] != '\0')
                           ? std::string(reinterpret_cast<const char *>(usb.chUserDefinedName))
                           : std::string(reinterpret_cast<const char *>(usb.chSerialNumber));
        }

        if (addCamera(cfg, &dev))
        {
            auto *cam = getCamera(cfg.id);
            cam->startGrabbing();
            SPDLOG_INFO("Camera {} ({}) started", cfg.id, cfg.name);
        }
    }
}

bool CameraManager::addCamera(const CameraParam &config)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.id))
    {
        SPDLOG_WARN("Camera {} already exists", config.id);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->open(config))
    {
        return false;
    }
    cameras_[config.id] = std::move(cam);
    return true;
}

bool CameraManager::addCamera(const CameraParam &config, MV_CC_DEVICE_INFO *dev_info)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.id))
    {
        SPDLOG_WARN("Camera {} already exists", config.id);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->open(config, dev_info))
    {
        return false;
    }
    cameras_[config.id] = std::move(cam);
    return true;
}

void CameraManager::removeCamera(const std::string &id)
{
    std::lock_guard lock(mutex_);
    if (auto it = cameras_.find(id); it != cameras_.end())
    {
        it->second->close();
        cameras_.erase(it);
    }
    offline_cameras_.erase(id);
}

ICamera *CameraManager::getCamera(const std::string &id)
{
    std::lock_guard lock(mutex_);
    auto it = cameras_.find(id);
    return it != cameras_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> CameraManager::cameraIds() const
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(cameras_.size());
    for (auto &[id, _] : cameras_)
    {
        ids.push_back(id);
    }
    return ids;
}

void CameraManager::openAll()
{
    std::lock_guard lock(mutex_);
    for (auto &[id, cam] : cameras_)
    {
        if (!cam->isOpened())
        {
            SPDLOG_WARN("Camera {} is not opened", id);
        }
    }
}

void CameraManager::closeAll()
{
    reconnect_timer_->stop();
    offline_cameras_.clear();

    std::lock_guard lock(mutex_);
    for (auto &[id, cam] : cameras_)
    {
        cam->close();
    }
    cameras_.clear();
}

void CameraManager::markOffline(const std::string &camera_id)
{
    {
        std::lock_guard lock(mutex_);
        auto it = cameras_.find(camera_id);
        if (it != cameras_.end())
        {
            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (hik_cam)
                hik_cam->forceClose();
        }
    }
    offline_cameras_.insert(camera_id);
    emit sign_cameraStatusChanged(camera_id, false);
    SPDLOG_INFO("Camera {} marked offline", camera_id);
}

void CameraManager::slot_reconnectTimer()
{
    // 只枚举一次，结果用于：心跳检测、离线重连、新设备发现
    auto devices = HikCamera::enumDevices();

    // 收集当前在线设备的 IP 集合
    std::set<std::string> online_ips;
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
            online_ips.insert(ip);
        }
    }

    // 收集状态变化，释放锁后再 emit（避免信号同步触发 slot 导致死锁）
    std::vector<std::pair<std::string, bool>> status_changes;

    // ── 1. 心跳检测：在线相机的 IP 不在枚举结果中 → 掉线 ──
    {
        std::lock_guard lock(mutex_);
        for (auto &[cam_id, cam] : cameras_)
        {
            if (offline_cameras_.count(cam_id))
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(cam.get());
            if (!hik_cam || !hik_cam->isOpened())
                continue;

            if (online_ips.find(hik_cam->config().ip) == online_ips.end())
            {
                SPDLOG_WARN("Camera {} (IP: {}) not found in enum, marking offline", cam_id, hik_cam->config().ip);
                hik_cam->forceClose();
                offline_cameras_.insert(cam_id);
                status_changes.emplace_back(cam_id, false);
            }
        }
    }

    // ── 2. 尝试重连离线相机（IP 重新出现在枚举结果中才尝试） ──
    if (!offline_cameras_.empty())
    {
        auto to_reconnect = offline_cameras_;
        for (const auto &cam_id : to_reconnect)
        {
            std::lock_guard lock(mutex_);
            auto it = cameras_.find(cam_id);
            if (it == cameras_.end())
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (!hik_cam)
                continue;

            if (online_ips.find(hik_cam->config().ip) == online_ips.end())
                continue;

            if (hik_cam->reconnect())
            {
                offline_cameras_.erase(cam_id);
                status_changes.emplace_back(cam_id, true);
            }
            else
            {
                SPDLOG_WARN("Camera {} reconnect failed, will retry", cam_id);
            }
        }
    }

    // ── 3. 发现尚未管理的新设备并自动添加 ──
    {
        size_t current_count;
        std::set<std::string> managed_ips;
        {
            std::lock_guard lock(mutex_);
            current_count = cameras_.size();
            for (auto &[id, cam] : cameras_)
            {
                auto *hik = dynamic_cast<HikCamera *>(cam.get());
                if (hik)
                    managed_ips.insert(hik->config().ip);
            }
        }

        for (auto &dev : devices)
        {
            if (current_count >= 4)
                break;

            std::string dev_ip;
            std::string dev_name;
            if (dev.nTLayerType == MV_GIGE_DEVICE)
            {
                auto &gige = dev.SpecialInfo.stGigEInfo;
                char ip[32];
                snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                         (gige.nCurrentIp >> 24) & 0xFF,
                         (gige.nCurrentIp >> 16) & 0xFF,
                         (gige.nCurrentIp >> 8) & 0xFF,
                         gige.nCurrentIp & 0xFF);
                dev_ip = ip;
                dev_name = (gige.chUserDefinedName[0] != '\0')
                               ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                               : dev_ip;
            }
            else if (dev.nTLayerType == MV_USB_DEVICE)
            {
                auto &usb = dev.SpecialInfo.stUsb3VInfo;
                dev_name = (usb.chUserDefinedName[0] != '\0')
                               ? std::string(reinterpret_cast<const char *>(usb.chUserDefinedName))
                               : std::string(reinterpret_cast<const char *>(usb.chSerialNumber));
            }

            if (managed_ips.count(dev_ip))
                continue;

            CameraParam cfg;
            cfg.id = "cam_" + std::to_string(current_count + 1);
            cfg.ip = dev_ip;
            cfg.name = dev_name;

            if (addCamera(cfg, &dev))
            {
                auto *cam = getCamera(cfg.id);
                if (cam)
                {
                    cam->startGrabbing();
                    status_changes.emplace_back(cfg.id, true);
                    SPDLOG_INFO("New camera discovered and started: {} ({})", cfg.id, cfg.name);
                }
                managed_ips.insert(dev_ip);
                ++current_count;
            }
        }
    }

    // ── 4. 释放锁，安全发送信号 ──
    for (auto &[cam_id, online] : status_changes)
    {
        emit sign_cameraStatusChanged(cam_id, online);
    }
}
