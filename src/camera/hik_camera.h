#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <atomic>
#include <mutex>

/// @brief Hik相机
class HikCamera : public ICamera
{
public:
    HikCamera();
    ~HikCamera() override;

    /// @brief 打开相机（按 config 中的 IP 枚举匹配）
    /// @param config
    /// @return
    bool open(const CameraConfig &config) override;

    /// @brief 直接用设备信息打开相机（免二次枚举）
    /// @param config 相机配置（id/name 等）
    /// @param dev_info 枚举得到的设备信息
    /// @return
    bool open(const CameraConfig &config, MV_CC_DEVICE_INFO *dev_info);

    /// @brief 关闭相机
    void close() override;

    /// @brief 相机是否打开
    /// @return
    bool isOpened() const override;

    /// @brief 开始抓图
    /// @return
    bool startGrabbing() override;

    /// @brief 停止抓图
    void stopGrabbing() override;

    /// @brief 是否正在抓图
    /// @return
    bool isGrabbing() const override;

    /// @brief 抓取一帧
    /// @param frame
    /// @param timeout_ms
    /// @return
    bool grabOne(cv::Mat &frame, int timeout_ms = 3000) override;

    /// @brief 设置曝光
    /// @param us
    /// @return
    bool setExposureTime(float us) override;

    /// @brief 设置增益
    /// @param gain
    /// @return
    bool setGain(float gain) override;

    /// @brief 获取曝光
    /// @param us
    /// @return
    bool getExposureTime(float &us) override;

    /// @brief 获取增益
    /// @param gain
    /// @return
    bool getGain(float &gain) override;

    /// @brief 设置触发模式
    /// @param mode
    /// @return
    bool setTriggerMode(int mode) override;

    /// @brief 获取触发模式
    /// @param mode
    /// @return
    bool getTriggerMode(int &mode) override;

    /// @brief 设置帧率
    /// @param fps
    /// @return
    bool setFrameRate(float fps) override;

    /// @brief 获取帧率
    /// @param fps
    /// @return
    bool getFrameRate(float &fps) override;

    /// @brief 设置像素格式
    /// @param format
    /// @return
    bool setPixelFormat(const std::string &format) override;

    /// @brief 获取像素格式
    /// @param format
    /// @return
    bool getPixelFormat(std::string &format) override;

    /// @brief 软触发
    /// @return
    bool softTrigger() override;

    std::string id() const override { return config_.id; }

    std::string name() const override { return config_.name; }

    /// @brief 设置回调
    /// @param cb
    void setCallback(ICameraCallback *cb) override { callback_ = cb; }

    /// @brief 枚举所有海康设备
    /// @return
    static std::vector<MV_CC_DEVICE_INFO> enumDevices();

private:
    static void __stdcall imageCallback(unsigned char *p_data,
                                        MV_FRAME_OUT_INFO_EX *p_frameinfo,
                                        void *p_user);
    cv::Mat convertToMat(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo);

    void *handle_ = nullptr;
    CameraConfig config_;
    std::atomic<bool> opened_{false};
    std::atomic<bool> grabbing_{false};
    ICameraCallback *callback_ = nullptr;
    std::mutex grab_mutex_;
};
