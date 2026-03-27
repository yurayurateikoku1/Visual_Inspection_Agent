#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <any>
#include <map>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <halconcpp/HalconCpp.h>

using json = nlohmann::json;

struct InspectionResult
{
    bool pass = true;
    std::string detail;
    double confidence = 0.0;
    int64_t timestamp_ms = 0;
};

/// 运行时上下文：在 Pipeline 各步骤间传递数据
struct NodeContext
{
    std::string camera_name;
    HalconCpp::HObject image;         // 原图
    HalconCpp::HObject display_image; // 叠加检测结果的显示图
    InspectionResult result;
    std::map<std::string, std::any> data; // 扩展数据（如 ROI 坐标）
};

struct CameraParam
{
    std::string name;
    std::string ip;
    float exposure_time = 10000.0f;
    float gain = 0.0f;
    int trigger_mode = 0; // 0=连续采集, 1=软触发, 2=硬触发
    int rotation_deg = 0; // 0, 90, 180, 270
};

/// @brief 通信协议类型
enum class CommProtocol
{
    ModbusTCP, // 网口 → PLC
    ModbusRTU, // 串口 → PLC/外设
};

/// @brief Modbus 地址定义（对应从站内存布局，参照 doc/c#.txt）
///        0X 区 = 线圈 (FC01 读 / FC05 写单个)，地址范围 0~4095
///        4X 区 = 保持寄存器 (FC03 读 / FC06 写单个 / FC10 写多个)，地址范围 0~4095
namespace ModbusAddr
{
    // ── 0X 线圈区：DI 输入信号（从 PLC 读） ──
    constexpr uint16_t DI0_STRIP_VISION = 0; // DI0 剥皮视觉信号
    constexpr uint16_t DI1_SHELL_VISION = 1; // DI1 胶壳视觉信号
    constexpr uint16_t DI2 = 2;              // DI2 预留
    constexpr uint16_t DI3 = 3;              // DI3 预留
    constexpr uint16_t DI4_BACKLIGHT1 = 4;   // DI4 启动背光源1
    constexpr uint16_t DI5_SIDELIGHT2 = 5;   // DI5 启动侧光源2
    constexpr uint16_t DI6_LIGHT3 = 6;       // DI6 启动光源3
    constexpr uint16_t DI7_LIGHT4 = 7;       // DI7 启动光源4

    // ── 0X 线圈区：DO 输出信号（写到 PLC，地址 500~507，对应 C# Modbus_0x[500+i]） ──
    constexpr uint16_t DO0_VISION_OK = 500;       // DO0 视觉OK信号
    constexpr uint16_t DO1_VISION_NG = 501;       // DO1 视觉NG信号
    constexpr uint16_t DO2 = 502;                 // DO2 预留
    constexpr uint16_t DO3 = 503;                 // DO3 预留
    constexpr uint16_t DO4_TRIG_BACKLIGHT1 = 504; // DO4 触发背光源1
    constexpr uint16_t DO5_TRIG_SIDELIGHT2 = 505; // DO5 触发侧光源2
    constexpr uint16_t DO6_TRIG_LIGHT3 = 506;     // DO6 触发光源3
    constexpr uint16_t DO7_TRIG_LIGHT4 = 507;     // DO7 触发光源4

    // ── 4X 保持寄存器区 ──
    constexpr uint16_t REG_RESULT = 0; // 检测结果: 1=OK, 0=NG
    constexpr uint16_t REG_STATUS = 1; // 系统状态
}

/// @brief 光源参数
struct LightParam
{
    std::string serial_port = "COM1"; // 光源控制器串口
    int baud_rate = 38400;            // 波特率（光源控制器固定38400）
    int luminance[4] = {0, 0, 0, 0};  // 4路通道亮度 0~255
    bool use_modbus = false;          // true=通过Modbus/PLC控制, false=串口直接控制（对应C# cbGYTX）
};

/// @brief 工作流参数（每个相机一条独立的检测流水线）
///        参照 C# MainTask 状态机：DI触发 → 软触发拍照 → 算法检测 → DO输出结果
struct WorkflowParam
{
    std::string name;        // 名称，如 "剥皮检测"
    std::string camera_name; // 绑定的相机名称
    std::string comm_name;   // 绑定的通信通道名称

    // DI 触发配置
    uint16_t trigger_di_addr = 0; // 触发信号的 DI 线圈地址（如 DI0=0, DI1=1）
    int trigger_delay_ms = 0;     // 触发后延时（ms），对应 C# TrigDelay

    // DO 输出配置
    uint16_t do_ok_addr = 500; // OK 信号的 DO 线圈地址（对应 C# Modbus_0x[500]）
    uint16_t do_ng_addr = 501; // NG 信号的 DO 线圈地址（对应 C# Modbus_0x[501]）
    int result_hold_ms = 100;  // 结果信号保持时间（ms），等 DI 恢复后清除

    // 算法链
    std::vector<std::string> algorithm_ids;          // 按顺序执行的算法ID列表
    std::vector<nlohmann::json> algorithm_params;    // 与 algorithm_ids 并行，每个算法的配置参数

    // 相机参数覆盖（检测时可能需要不同曝光，参照 C# 不同检测切换曝光）
    float exposure_time = -1.0f; // <=0 表示不覆盖，使用相机默认值
};

struct CommunicationParam
{
    std::string name;
    CommProtocol protocol = CommProtocol::ModbusTCP;

    // TCP
    std::string ip;
    int port = 502;

    // RTU
    std::string serial_port = "COM1";
    int baud_rate = 9600;
    int data_bits = 8;
    int stop_bits = 1; // 1 或 2
    int parity = 0;    // 0=None, 1=Even, 2=Odd
    int slave_address = 1;
};
