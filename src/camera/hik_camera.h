#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <halconcpp/HalconCpp.h>
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
    bool open(const CameraParam &config) override;

    /// @brief 直接用设备信息打开相机（免二次枚举）
    /// @param config 相机配置（id/name 等）
    /// @param dev_info 枚举得到的设备信息
    /// @return
    bool open(const CameraParam &config, MV_CC_DEVICE_INFO *dev_info);

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
    bool grabOne(HalconCpp::HObject &frame, int timeout_ms = 3000) override;

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

    std::string getId() const override { return config_.id; }

    std::string getName() const override { return config_.name; }

    /// @brief 设置回调
    /// @param cb
    void setCallback(ICameraCallback *cb) override { callback_ = cb; }

    /// @brief 获取相机配置（重连时用）
    const CameraParam &config() const { return config_; }

    /// @brief 尝试重连（关闭旧连接 → 重新枚举 → 打开 → 开始采集）
    /// @return true 重连成功
    bool reconnect();

    /// @brief 检测相机是否仍然在线（通过枚举设备检查 IP 是否存在）
    /// @return true 在线
    bool isAlive() const;

    /// @brief 强制关闭（无论状态标记如何，强制关闭SDK 资源释放）
    void forceClose();

    /// @brief 枚举所有海康设备
    /// @return
    static std::vector<MV_CC_DEVICE_INFO> enumDevices();

private:
    /// @brief 一帧图像回调
    /// @param p_data
    /// @param p_frameinfo
    /// @param p_user
    /// @return
    static void __stdcall imageCallback(unsigned char *p_data,
                                        MV_FRAME_OUT_INFO_EX *p_frameinfo,
                                        void *p_user);
    /// @brief 异常回调
    /// @param msg_type
    /// @param p_user
    /// @return
    static void __stdcall exceptionCallback(unsigned int msg_type, void *p_user);

    /// @brief 转换为 Halcon HObject 图像
    HalconCpp::HObject convertToHobject(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo);

    void *handle_ = nullptr;              // 相机句柄
    CameraParam config_;                  // 相机配置
    std::atomic<bool> opened_{false};     // 相机是否打开
    std::atomic<bool> grabbing_{false};   // 是否正在抓图
    ICameraCallback *callback_ = nullptr; // 指向 ICameraCallback
    std::mutex handle_mutex_;
};
