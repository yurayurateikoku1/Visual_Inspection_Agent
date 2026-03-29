#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <any>
#include <map>
#include <variant>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <halconcpp/HalconCpp.h>

using json = nlohmann::json;

/// @brief 单个缺陷（检测器负责填充）
struct Defect
{
    std::string label; // 缺陷类型，如 "missing", "offset"
    float confidence = 0.0f;
    // 位置：矩形框 [row1, col1, row2, col2]
    float row1 = 0, col1 = 0, row2 = 0, col2 = 0;
};

/// @brief 检测结果
struct InspectionResult
{
    bool pass = true;
    std::vector<Defect> defects; // 具体缺陷列表，pass 由检测器根据业务规则判断
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

enum class DetectorType
{
    None,
    Terminal,
};

/// @brief 端子检测参数
struct TerminalParam
{
    bool enabled = false;
    std::string model_path;
    float score_threshold = 0.5f;
    float nms_threshold = 0.5f;
    std::string task_type = "YOLO_DET"; // "YOLO_DET" | "YOLO_OBB"
    bool end2end = false;
};

/// @brief 工作流参数（每个相机最多4条，对应 DI0~DI3 触发，每条独立算法链）
///        参照 C# MainTask 状态机：DI触发 → 软触发拍照 → 算法检测 → DO输出结果

/// @brief DI 触发配置
struct TriggerConfig
{
    uint16_t di_addr = 0; // 触发 DI 线圈地址（0~3）
    int delay_ms = 0;     // 触发后延时
};

/// @brief DO 输出配置
struct IoConfig
{
    uint16_t do_ok_addr = 500;
    uint16_t do_ng_addr = 501;
    int result_hold_ms = 100;
};

/// @brief 相机参数覆盖（<=0 表示不覆盖）
struct CameraOverride
{
    float exposure_time = -1.0f;
};

/// @brief ROI 裁剪（通用前处理）
struct RoiParam
{
    bool enabled = false;
    double row1 = 0, col1 = 0, row2 = 0, col2 = 0;
};

struct WorkflowParam
{
    std::string camera_name; // 绑定相机
    std::string comm_name;   // 绑定通信通道
    bool enabled = false;

    TriggerConfig trigger;
    IoConfig io;
    CameraOverride camera_override;
    RoiParam roi;

    /// 检测器参数：monostate = 不检测，TerminalParam = 端子检测
    std::variant<std::monostate, TerminalParam> detector_param;

    /// @brief workflow map key：camera_name + "_" + di_addr
    std::string key() const { return camera_name + "_" + std::to_string(trigger.di_addr); }
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
