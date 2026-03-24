#include "camera_manager.h"
#include "hik_camera.h"
#include <spdlog/spdlog.h>

CameraManager &CameraManager::getInstance()
{
    static CameraManager inst;
    return inst;
}

bool CameraManager::addCamera(const CameraConfig &config)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.id))
    {
        spdlog::warn("Camera {} already exists", config.id);
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

bool CameraManager::addCamera(const CameraConfig &config, MV_CC_DEVICE_INFO *dev_info)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(config.id))
    {
        spdlog::warn("Camera {} already exists", config.id);
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
            spdlog::warn("Camera {} is not opened", id);
        }
    }
}

void CameraManager::closeAll()
{
    std::lock_guard lock(mutex_);
    for (auto &[id, cam] : cameras_)
    {
        cam->close();
    }
    cameras_.clear();
}
