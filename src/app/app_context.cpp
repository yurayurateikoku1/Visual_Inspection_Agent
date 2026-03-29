#include "app_context.h"

AppContext &AppContext::getInstance()
{
    static AppContext inst;
    return inst;
}

AppContext::AppContext()
    : executor_(8)
{
    // ── 默认相机参数 ──
    CameraParam cam1;
    cam1.name = "ccd1";
    cam1.ip = "192.168.2.100";
    cam1.exposure_time = 10000.0f;
    cam1.gain = 0.0f;
    cam1.trigger_mode = 0;
    cam1.rotation_deg = 0;
    camera_params_[cam1.name] = cam1;

    // ── 默认通信参数 ──
    CommunicationParam comm1;
    comm1.name = "plc_1";
    comm1.protocol = CommProtocol::ModbusTCP;
    comm1.ip = "192.168.1.200";
    comm1.port = 502;
    comm1.slave_address = 1;
    comm_params_[comm1.name] = comm1;

    // ── 默认光源参数 ──
    light_param_.serial_port = "COM1";
    light_param_.baud_rate = 38400;
    light_param_.use_modbus = false;
    light_param_.luminance[0] = 0;
    light_param_.luminance[1] = 0;
    light_param_.luminance[2] = 0;
    light_param_.luminance[3] = 0;

    // ── 默认工作流参数 ──
    WorkflowParam wf1;
    wf1.camera_name     = "ccd1";
    wf1.comm_name       = "plc_1";
    wf1.trigger.di_addr = 0;
    wf1.trigger.delay_ms = 0;
    wf1.io.do_ok_addr      = 500;
    wf1.io.do_ng_addr      = 501;
    wf1.io.result_hold_ms  = 100;
    wf1.camera_override.exposure_time   = -1.0f;
    workflow_params[wf1.key()] = wf1;
}
