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

bool HikCamera::open(const CameraConfig &config)
{
    if (opened_)
    {
        SPDLOG_WARN("Camera {} already opened", config.id);
        return true;
    }

    // 枚举设备并按 IP 匹配
    MV_CC_DEVICE_INFO_LIST device_list{};
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK || device_list.nDeviceNum == 0)
    {
        SPDLOG_ERROR("No devices found for camera {}", config.id);
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
        SPDLOG_ERROR("Camera {} with IP {} not found", config.id, config.ip);
        return false;
    }

    return open(config, target);
}

bool HikCamera::open(const CameraConfig &config, MV_CC_DEVICE_INFO *dev_info)
{
    if (opened_)
    {
        SPDLOG_WARN("Camera {} already opened", config.id);
        return true;
    }
    config_ = config;

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

    setExposureTime(config.exposure_time);
    setGain(config.gain);
    setTriggerMode(config.trigger_mode);

    opened_ = true;
    SPDLOG_INFO("Camera {} opened (name: {})", config.id, config.name);
    return true;
}

void HikCamera::close()
{
    if (grabbing_)
        stopGrabbing();
    if (handle_)
    {
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }
    opened_ = false;
}

bool HikCamera::isOpened() const { return opened_; }

bool HikCamera::startGrabbing()
{
    if (!opened_ || grabbing_)
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
    if (!grabbing_)
        return;
    MV_CC_StopGrabbing(handle_);
    grabbing_ = false;
}

bool HikCamera::isGrabbing() const { return grabbing_; }

bool HikCamera::grabOne(cv::Mat &frame, int timeout_ms)
{
    if (!opened_)
        return false;
    std::lock_guard lock(grab_mutex_);

    MV_FRAME_OUT frame_out{};
    int ret = MV_CC_GetImageBuffer(handle_, &frame_out, timeout_ms);
    if (ret != MV_OK)
    {
        spdlog::error("GetImageBuffer failed: 0x{:08X}", ret);
        return false;
    }

    frame = convertToMat(frame_out.pBufAddr, &frame_out.stFrameInfo);
    MV_CC_FreeImageBuffer(handle_, &frame_out);
    return !frame.empty();
}

bool HikCamera::setExposureTime(float us)
{
    if (!handle_)
        return false;
    MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
    int ret = MV_CC_SetFloatValue(handle_, "ExposureTime", us);
    return ret == MV_OK;
}

bool HikCamera::getExposureTime(float &us)
{
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
    if (!handle_)
        return false;
    MV_CC_SetEnumValue(handle_, "GainAuto", 0);
    int ret = MV_CC_SetFloatValue(handle_, "Gain", gain);
    return ret == MV_OK;
}

bool HikCamera::getGain(float &gain)
{
    if (!handle_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handle_, "Gain", &val);
    if (ret != MV_OK)
        return false;
    gain = val.fCurValue;
    return true;
}

bool HikCamera::setTriggerMode(int mode)
{
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
    if (!handle_)
        return false;
    MV_CC_SetBoolValue(handle_, "AcquisitionFrameRateEnable", true);
    int ret = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", fps);
    return ret == MV_OK;
}

bool HikCamera::getFrameRate(float &fps)
{
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
    if (!handle_)
        return false;
    // 映射常用像素格式字符串到海康枚举值
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
    int ret = MV_CC_SetEnumValue(handle_, "PixelFormat", it->second);
    return ret == MV_OK;
}

bool HikCamera::getPixelFormat(std::string &format)
{
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
    if (!handle_)
        return false;
    return MV_CC_SetCommandValue(handle_, "TriggerSoftware") == MV_OK;
}

void __stdcall HikCamera::imageCallback(unsigned char *p_data,
                                        MV_FRAME_OUT_INFO_EX *p_frameinfo,
                                        void *p_user)
{
    auto *self = static_cast<HikCamera *>(p_user);
    if (self->callback_ && p_data && p_frameinfo)
    {
        cv::Mat frame = self->convertToMat(p_data, p_frameinfo);
        if (!frame.empty())
        {
            self->callback_->onFrameReceived(self->config_.id, frame);
        }
    }
}

cv::Mat HikCamera::convertToMat(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo)
{
    int width = p_frameinfo->nWidth;
    int height = p_frameinfo->nHeight;

    if (p_frameinfo->enPixelType == PixelType_Gvsp_Mono8)
    {
        return cv::Mat(height, width, CV_8UC1, p_data).clone();
    }

    // 其他格式转为 BGR
    MV_CC_PIXEL_CONVERT_PARAM param{};
    param.nWidth = width;
    param.nHeight = height;
    param.enSrcPixelType = p_frameinfo->enPixelType;
    param.pSrcData = p_data;
    param.nSrcDataLen = p_frameinfo->nFrameLen;
    param.enDstPixelType = PixelType_Gvsp_BGR8_Packed;

    cv::Mat bgr(height, width, CV_8UC3);
    param.pDstBuffer = bgr.data;
    param.nDstBufferSize = bgr.total() * bgr.elemSize();

    int ret = MV_CC_ConvertPixelType(handle_, &param);
    if (ret != MV_OK)
    {
        spdlog::error("ConvertPixelType failed: 0x{:08X}", ret);
        return {};
    }
    return bgr;
}
