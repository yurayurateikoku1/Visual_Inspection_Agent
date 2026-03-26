#pragma once

#include <string>
#include <functional>
#include <halconcpp/HalconCpp.h>
#include "../app/common.h"

class ICameraCallback
{
public:
    virtual ~ICameraCallback() = default;
    virtual void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) = 0;
    virtual void cameraErrorReceived(const std::string &camera_name, int error_code, const std::string &msg) = 0;
};

/// @brief 相机接口
class ICamera
{
public:
    virtual ~ICamera() = default;

    /// @brief 打开相机
    /// @param config
    /// @return
    virtual bool open(const CameraParam &config) = 0;

    /// @brief 关闭相机
    virtual void close() = 0;

    /// @brief 相机是否打开
    /// @return
    virtual bool isOpened() const = 0;

    /// @brief 开始抓图
    /// @return
    virtual bool startGrabbing() = 0;

    /// @brief 停止抓图
    virtual void stopGrabbing() = 0;

    /// @brief 是否正在抓图
    /// @return
    virtual bool isGrabbing() const = 0;

    /// @brief 抓取一帧
    /// @param frame
    /// @param timeout_ms
    /// @return
    virtual bool grabOne(HalconCpp::HObject &frame, int timeout_ms = 3000) = 0;

    /// @brief 设置曝光
    /// @param us
    /// @return
    virtual bool setExposureTime(float us) = 0;

    /// @brief 获取曝光
    /// @param us
    /// @return
    virtual bool getExposureTime(float &us) = 0;

    /// @brief 设置增益
    /// @param gain
    /// @return
    virtual bool setGain(float gain) = 0;

    /// @brief 获取增益
    /// @param gain
    /// @return
    virtual bool getGain(float &gain) = 0;

    /// @brief 设置触发模式 (0=连续采集, 1=软触发, 2=硬触发)
    /// @param mode
    /// @return
    virtual bool setTriggerMode(int mode) = 0;

    /// @brief 获取触发模式
    /// @param mode
    /// @return
    virtual bool getTriggerMode(int &mode) = 0;

    /// @brief 设置帧率
    /// @param fps
    /// @return
    virtual bool setFrameRate(float fps) = 0;

    /// @brief 获取帧率
    /// @param fps
    /// @return
    virtual bool getFrameRate(float &fps) = 0;

    /// @brief 设置像素格式 (如 "Mono8", "BayerRG8", "RGB8Packed" 等)
    /// @param format
    /// @return
    virtual bool setPixelFormat(const std::string &format) = 0;

    /// @brief 获取像素格式
    /// @param format
    /// @return
    virtual bool getPixelFormat(std::string &format) = 0;

    /// @brief 软触发
    /// @return
    virtual bool softTrigger() = 0;

    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;

    /// @brief 设置回调
    /// @param cb
    virtual void setCallback(ICameraCallback *cb) = 0;
};
