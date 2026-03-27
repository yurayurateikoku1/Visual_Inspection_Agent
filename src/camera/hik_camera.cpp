#include "hik_camera.h"
#include <spdlog/spdlog.h>
#include <unordered_map>

HikCamera::HikCamera() = default;

HikCamera::~HikCamera()
{
    close();
}

std::vector<MV_CC_DEVICE_INFO> HikCamera::enumDevices()
{
    std::vector<MV_CC_DEVICE_INFO> result;
    MV_CC_DEVICE_INFO_LIST device_list{};
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("EnumDevices failed: 0x{:08X}", ret);
        return result;
    }
    for (unsigned int i = 0; i < device_list.nDeviceNum; ++i)
    {
        if (device_list.pDeviceInfo[i])
        {
            result.push_back(*device_list.pDeviceInfo[i]);
        }
    }
    SPDLOG_INFO("Found {} HIK devices", result.size());
    return result;
}

bool HikCamera::open(const CameraParam &config)
{
    if (opened_)
    {
        SPDLOG_WARN("Camera {} already opened", config.name);
        return true;
    }

    // 枚举设备并按 IP 匹配
    MV_CC_DEVICE_INFO_LIST device_list{};
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK || device_list.nDeviceNum == 0)
    {
        SPDLOG_ERROR("No devices found for camera {}", config.name);
        return false;
    }

    MV_CC_DEVICE_INFO *target = nullptr;
    for (unsigned int i = 0; i < device_list.nDeviceNum; ++i)
    {
        auto *info = device_list.pDeviceInfo[i];
        if (!info)
            continue;
        if (info->nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = info->SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            if (config.ip == ip)
            {
                target = info;
                break;
            }
        }
    }

    if (!target)
    {
        SPDLOG_ERROR("Camera {} with IP {} not found", config.name, config.ip);
        return false;
    }

    return open(config, target);
}

bool HikCamera::open(const CameraParam &config, MV_CC_DEVICE_INFO *dev_info)
{
    if (opened_)
    {
        SPDLOG_WARN("Camera {} already opened", config.name);
        return true;
    }
    camera_param_ = config;

    int ret = MV_CC_CreateHandle(&handle_, dev_info);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("CreateHandle failed: 0x{:08X}", ret);
        return false;
    }

    ret = MV_CC_OpenDevice(handle_);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("OpenDevice failed: 0x{:08X}", ret);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    // 注册异常回调（掉线检测）
    MV_CC_RegisterExceptionCallBack(handle_, exceptionCallback, this);

    setExposureTime(config.exposure_time);
    setGain(config.gain);
    setTriggerMode(config.trigger_mode);

    opened_ = true;
    SPDLOG_INFO("Camera {} opened", config.name);
    return true;
}

void HikCamera::close()
{
    void *h = handle_;
    if (!h)
        return;

    // 1. 标记关闭 — imageCallback 检查后直接 return，不再持锁
    opened_ = false;
    grabbing_ = false;

    // 2. 停止抓取（SDK 内部会等待回调退出）
    MV_CC_StopGrabbing(h);

    // 3. 回调已全部退出，安全释放资源
    std::lock_guard lock(handle_mutex_);
    if (handle_)
    {
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }
}

void HikCamera::forceClose()
{
    void *h = handle_;
    if (!h)
    {
        opened_ = false;
        grabbing_ = false;
        return;
    }

    opened_ = false;
    grabbing_ = false;

    MV_CC_StopGrabbing(h);

    std::lock_guard lock(handle_mutex_);
    if (handle_)
    {
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }
}

bool HikCamera::isOpened() const { return opened_; }

bool HikCamera::startGrabbing()
{
    if (!opened_ || grabbing_)
        return false;

    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;

    int ret = MV_CC_RegisterImageCallBackEx(handle_, imageCallback, this);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("RegisterImageCallback failed: 0x{:08X}", ret);
        return false;
    }

    ret = MV_CC_StartGrabbing(handle_);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("StartGrabbing failed: 0x{:08X}", ret);
        return false;
    }
    grabbing_ = true;
    return true;
}

void HikCamera::stopGrabbing()
{
    if (!grabbing_ || !handle_)
        return;
    grabbing_ = false;
    MV_CC_StopGrabbing(handle_);
}

bool HikCamera::isGrabbing() const { return grabbing_; }

bool HikCamera::grabOne(HalconCpp::HObject &frame, int timeout_ms)
{
    if (!opened_)
        return false;
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;

    MV_FRAME_OUT frame_out{};
    int ret = MV_CC_GetImageBuffer(handle_, &frame_out, timeout_ms);
    if (ret != MV_OK)
    {
        spdlog::error("GetImageBuffer failed: 0x{:08X}", ret);
        return false;
    }

    frame = convertToHobject(frame_out.pBufAddr, &frame_out.stFrameInfo);
    MV_CC_FreeImageBuffer(handle_, &frame_out);
    return frame.IsInitialized();
}

bool HikCamera::setExposureTime(float us)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
    int ret = MV_CC_SetFloatValue(handle_, "ExposureTime", us);
    return ret == MV_OK;
}

bool HikCamera::getExposureTime(float &us)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handle_, "ExposureTime", &val);
    if (ret != MV_OK)
        return false;
    us = val.fCurValue;
    return true;
}

bool HikCamera::setGain(float gain)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MV_CC_SetEnumValue(handle_, "GainAuto", 0);
    int ret = MV_CC_SetFloatValue(handle_, "Gain", gain);
    if (ret != MV_OK)
    {
        // 部分型号使用 GainRaw 节点
        ret = MV_CC_SetFloatValue(handle_, "GainRaw", gain);
    }
    return ret == MV_OK;
}

bool HikCamera::getGain(float &gain)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handle_, "Gain", &val);
    if (ret != MV_OK)
    {
        // 部分型号使用 GainRaw 节点
        ret = MV_CC_GetFloatValue(handle_, "GainRaw", &val);
    }
    if (ret != MV_OK)
    {
        SPDLOG_WARN("getGain failed: 0x{:08X}", ret);
        return false;
    }
    gain = val.fCurValue;
    return true;
}

bool HikCamera::setTriggerMode(int mode)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    if (mode == 0)
    {
        MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
    }
    else
    {
        MV_CC_SetEnumValue(handle_, "TriggerMode", 1);
        MV_CC_SetEnumValue(handle_, "TriggerSource",
                           mode == 1 ? 7 /*Software*/ : 0 /*Line0*/);
    }
    return true;
}

bool HikCamera::getTriggerMode(int &mode)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MVCC_ENUMVALUE val{};
    int ret = MV_CC_GetEnumValue(handle_, "TriggerMode", &val);
    if (ret != MV_OK)
        return false;
    if (val.nCurValue == 0)
    {
        mode = 0; // 连续采集
    }
    else
    {
        MVCC_ENUMVALUE src{};
        MV_CC_GetEnumValue(handle_, "TriggerSource", &src);
        mode = (src.nCurValue == 7) ? 1 : 2; // 7=Software, 0=Line0
    }
    return true;
}

bool HikCamera::setFrameRate(float fps)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MV_CC_SetBoolValue(handle_, "AcquisitionFrameRateEnable", true);
    int ret = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", fps);
    return ret == MV_OK;
}

bool HikCamera::getFrameRate(float &fps)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handle_, "AcquisitionFrameRate", &val);
    if (ret != MV_OK)
        return false;
    fps = val.fCurValue;
    return true;
}

bool HikCamera::setPixelFormat(const std::string &format)
{
    // 格式映射不需要锁
    static const std::unordered_map<std::string, unsigned int> fmt_map = {
        {"Mono8", PixelType_Gvsp_Mono8},
        {"Mono10", PixelType_Gvsp_Mono10},
        {"Mono12", PixelType_Gvsp_Mono12},
        {"BayerRG8", PixelType_Gvsp_BayerRG8},
        {"BayerGB8", PixelType_Gvsp_BayerGB8},
        {"BayerGR8", PixelType_Gvsp_BayerGR8},
        {"BayerBG8", PixelType_Gvsp_BayerBG8},
        {"RGB8Packed", PixelType_Gvsp_RGB8_Packed},
        {"YUV422_8", PixelType_Gvsp_YUV422_Packed},
    };
    auto it = fmt_map.find(format);
    if (it == fmt_map.end())
    {
        SPDLOG_ERROR("Unknown pixel format: {}", format);
        return false;
    }
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    int ret = MV_CC_SetEnumValue(handle_, "PixelFormat", it->second);
    return ret == MV_OK;
}

bool HikCamera::getPixelFormat(std::string &format)
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    MVCC_ENUMVALUE val{};
    int ret = MV_CC_GetEnumValue(handle_, "PixelFormat", &val);
    if (ret != MV_OK)
        return false;
    // 反向映射枚举值到字符串
    static const std::unordered_map<unsigned int, std::string> fmt_map = {
        {PixelType_Gvsp_Mono8, "Mono8"},
        {PixelType_Gvsp_Mono10, "Mono10"},
        {PixelType_Gvsp_Mono12, "Mono12"},
        {PixelType_Gvsp_BayerRG8, "BayerRG8"},
        {PixelType_Gvsp_BayerGB8, "BayerGB8"},
        {PixelType_Gvsp_BayerGR8, "BayerGR8"},
        {PixelType_Gvsp_BayerBG8, "BayerBG8"},
        {PixelType_Gvsp_RGB8_Packed, "RGB8Packed"},
        {PixelType_Gvsp_YUV422_Packed, "YUV422_8"},
    };
    auto it = fmt_map.find(val.nCurValue);
    if (it != fmt_map.end())
        format = it->second;
    else
        format = "Unknown(" + std::to_string(val.nCurValue) + ")";
    return true;
}

bool HikCamera::softTrigger()
{
    std::lock_guard lock(handle_mutex_);
    if (!handle_)
        return false;
    return MV_CC_SetCommandValue(handle_, "TriggerSoftware") == MV_OK;
}

void __stdcall HikCamera::imageCallback(unsigned char *p_data,
                                        MV_FRAME_OUT_INFO_EX *p_frameinfo,
                                        void *p_user)
{
    auto *self = static_cast<HikCamera *>(p_user);
    if (!self->opened_ || !self->callback_ || !p_data || !p_frameinfo)
        return;

    try
    {
        HalconCpp::HObject frame;
        {
            std::lock_guard lock(self->handle_mutex_);
            if (!self->handle_)
                return;
            frame = self->convertToHobject(p_data, p_frameinfo);
        }
        if (frame.IsInitialized())
        {
            self->callback_->frameReceived(self->camera_param_.name, frame);
        }
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("imageCallback HException: {}", e.ErrorMessage().Text());
    }
    catch (std::exception &e)
    {
        SPDLOG_ERROR("imageCallback exception: {}", e.what());
    }
}

void __stdcall HikCamera::exceptionCallback(unsigned int msg_type, void *p_user)
{
    auto *self = static_cast<HikCamera *>(p_user);
    SPDLOG_ERROR("Camera {} exception: 0x{:08X}", self->camera_param_.name, msg_type);
    // 不在 SDK 回调线程中直接操作 handle_ 或修改状态，
    // 仅通知上层（MainWindow::onCameraError 会投递到主线程调 markOffline）
    if (self->callback_)
    {
        self->callback_->cameraErrorReceived(self->camera_param_.name, static_cast<int>(msg_type), "device exception/disconnected");
    }
}

bool HikCamera::isAlive() const
{
    if (!handle_ || !opened_)
        return false;
    // 通过枚举设备检查本相机 IP 是否仍在线（读参数可能返回缓存值，不可靠）
    auto devices = enumDevices();
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
            if (camera_param_.ip == ip)
                return true;
        }
    }
    return false;
}

bool HikCamera::reconnect()
{
    SPDLOG_INFO("Camera {} attempting reconnect...", camera_param_.name);

    // 1. 强制清理旧连接（无论状态标记如何，确保 SDK 资源释放）
    forceClose();

    // 2. 重新枚举，查找匹配 IP 的设备
    auto devices = enumDevices();
    MV_CC_DEVICE_INFO *target = nullptr;
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
            if (camera_param_.ip == ip)
            {
                target = &dev;
                break;
            }
        }
    }

    if (!target)
    {
        SPDLOG_WARN("Camera {} (IP: {}) not found during reconnect", camera_param_.name, camera_param_.ip);
        return false;
    }

    // 3. 重新打开
    if (!open(camera_param_, target))
        return false;

    // 4. 重新开始采集
    if (!startGrabbing())
    {
        close();
        return false;
    }

    SPDLOG_INFO("Camera {} reconnected successfully", camera_param_.name);
    return true;
}

HalconCpp::HObject HikCamera::convertToHobject(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo)
{
    int width = p_frameinfo->nWidth;
    int height = p_frameinfo->nHeight;
    auto pixel_type = p_frameinfo->enPixelType;
    HalconCpp::HObject image;

    // Mono8：直接生成单通道图像
    if (pixel_type == PixelType_Gvsp_Mono8)
    {
        HalconCpp::GenImage1(&image, "byte", width, height, reinterpret_cast<Hlong>(p_data));
        return image;
    }

    // Bayer 格式：先生成单通道原始图，再用 Halcon CfaToRgb 转彩色
    if (pixel_type == PixelType_Gvsp_BayerRG8 ||
        pixel_type == PixelType_Gvsp_BayerGB8 ||
        pixel_type == PixelType_Gvsp_BayerGR8 ||
        pixel_type == PixelType_Gvsp_BayerBG8)
    {
        try
        {
            HalconCpp::HObject bayer_image;
            HalconCpp::GenImage1(&bayer_image, "byte", width, height, reinterpret_cast<Hlong>(p_data));

            // Halcon Bayer 模式字符串
            const char *cfa = "bayer_rg"; // 默认
            if (pixel_type == PixelType_Gvsp_BayerGB8)
                cfa = "bayer_gb";
            else if (pixel_type == PixelType_Gvsp_BayerGR8)
                cfa = "bayer_gr";
            else if (pixel_type == PixelType_Gvsp_BayerBG8)
                cfa = "bayer_bg";

            HalconCpp::CfaToRgb(bayer_image, &image, cfa, "bilinear");
            return image;
        }
        catch (HalconCpp::HException &e)
        {
            SPDLOG_ERROR("CfaToRgb failed: {}", e.ErrorMessage().Text());
            return {};
        }
    }

    // RGB8Packed：直接拆分 R/G/B 平面
    if (pixel_type == PixelType_Gvsp_RGB8_Packed)
    {
        std::vector<unsigned char> r(width * height), g(width * height), b(width * height);
        for (int i = 0; i < width * height; ++i)
        {
            r[i] = p_data[i * 3];
            g[i] = p_data[i * 3 + 1];
            b[i] = p_data[i * 3 + 2];
        }
        HalconCpp::GenImage3(&image, "byte", width, height,
                             reinterpret_cast<Hlong>(r.data()),
                             reinterpret_cast<Hlong>(g.data()),
                             reinterpret_cast<Hlong>(b.data()));
        return image;
    }

    // 其他格式：尝试 SDK 转 RGB8
    MV_CC_PIXEL_CONVERT_PARAM param{};
    param.nWidth = width;
    param.nHeight = height;
    param.enSrcPixelType = pixel_type;
    param.pSrcData = p_data;
    param.nSrcDataLen = p_frameinfo->nFrameLen;
    param.enDstPixelType = PixelType_Gvsp_RGB8_Packed;

    std::vector<unsigned char> rgb_buf(width * height * 3);
    param.pDstBuffer = rgb_buf.data();
    param.nDstBufferSize = static_cast<unsigned int>(rgb_buf.size());

    int ret = MV_CC_ConvertPixelType(handle_, &param);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("ConvertPixelType failed: pixelType=0x{:08X}, ret=0x{:08X}",
                     static_cast<unsigned int>(pixel_type), ret);
        return {};
    }

    std::vector<unsigned char> r(width * height), g(width * height), b(width * height);
    for (int i = 0; i < width * height; ++i)
    {
        r[i] = rgb_buf[i * 3];
        g[i] = rgb_buf[i * 3 + 1];
        b[i] = rgb_buf[i * 3 + 2];
    }

    HalconCpp::GenImage3(&image, "byte", width, height,
                         reinterpret_cast<Hlong>(r.data()),
                         reinterpret_cast<Hlong>(g.data()),
                         reinterpret_cast<Hlong>(b.data()));
    return image;
}
