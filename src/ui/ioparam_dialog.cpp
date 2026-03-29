#include "ioparam_dialog.h"
#include "ui_ioparam_dialog.h"
#include "../app/common.h"
#include "../app/app_context.h"
#include "../app/config_manager.h"
#include "../communication/comm_manager.h"
#include <spdlog/spdlog.h>

IOParamDialog::IOParamDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::IOParamDialog)
{
    ui->setupUi(this);

    // DI CheckBox 数组，输入只读
    di_checkboxes_ = {
        ui->checkBox_DI0, ui->checkBox_DI1, ui->checkBox_DI2, ui->checkBox_DI3,
        ui->checkBox_DI4, ui->checkBox_DI5, ui->checkBox_DI6, ui->checkBox_DI7};
    for (auto *cb : di_checkboxes_)
        cb->setEnabled(false);

    // DO CheckBox 数组，输出可写
    do_checkboxes_ = {
        ui->checkBox_DO0, ui->checkBox_DO1, ui->checkBox_DO2, ui->checkBox_DO3,
        ui->checkBox_DO4, ui->checkBox_DO5, ui->checkBox_DO6, ui->checkBox_DO7};
    for (int i = 0; i < 8; ++i)
    {
        connect(do_checkboxes_[i], &QCheckBox::clicked, this, [this, i](bool checked)
                { slot_doCheckBoxClicked(i, checked); });
    }

    // 监听 CommManager 的 IO 状态更新信号
    connect(&CommManager::getInstance(), &CommManager::sign_ioStateUpdated,
            this, &IOParamDialog::slot_updateIODisplay);

    loadParam();
}

IOParamDialog::~IOParamDialog()
{
    delete ui;
}

void IOParamDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    loadParam();
}

void IOParamDialog::loadParam()
{
    // ── 通信参数 ──
    auto &cfgs = AppContext::getInstance().comm_params;
    if (!cfgs.empty())
    {
        auto &cfg = cfgs.begin()->second;
        ui->spinBox_comPort->setValue(
            cfg.serial_port.size() > 3 ? std::stoi(cfg.serial_port.substr(3)) : 1);
        ui->lineEdit_ipAddress->setText(QString::fromStdString(cfg.ip));
        ui->spinBox_ipPort->setValue(cfg.port);

        if (cfg.protocol == CommProtocol::ModbusRTU)
            ui->radioButton_rtu->setChecked(true);
        else
            ui->radioButton_tcp->setChecked(true);
    }

    // ── 光源参数
    auto &lp = AppContext::getInstance().light_param;
    ui->spinBox_lightPort->setValue(
        lp.serial_port.size() > 3 ? std::stoi(lp.serial_port.substr(3)) : 1);
    ui->spinBox_ch1Brightness->setValue(lp.luminance[0]);
    ui->spinBox_ch2Brightness->setValue(lp.luminance[1]);
    ui->spinBox_ch3Brightness->setValue(lp.luminance[2]);
    ui->spinBox_ch4Brightness->setValue(lp.luminance[3]);
    ui->checkBox_lightSControl->setChecked(lp.use_modbus);
}

void IOParamDialog::saveParam()
{
    auto &ctx = AppContext::getInstance();

    // ── 通信参数 ──
    auto &cfgs = ctx.comm_params;
    if (cfgs.empty())
    {
        CommunicationParam cp;
        cp.name = "plc_1";
        cfgs["plc_1"] = cp;
    }
    auto &cfg = cfgs.begin()->second;
    cfg.serial_port = "COM" + std::to_string(ui->spinBox_comPort->value());
    cfg.ip = ui->lineEdit_ipAddress->text().toStdString();
    cfg.port = ui->spinBox_ipPort->value();
    cfg.protocol = ui->radioButton_rtu->isChecked() ? CommProtocol::ModbusRTU : CommProtocol::ModbusTCP;

    // ── 光源参数
    auto &lp = ctx.light_param;
    lp.serial_port = "COM" + std::to_string(ui->spinBox_lightPort->value());
    lp.luminance[0] = ui->spinBox_ch1Brightness->value();
    lp.luminance[1] = ui->spinBox_ch2Brightness->value();
    lp.luminance[2] = ui->spinBox_ch3Brightness->value();
    lp.luminance[3] = ui->spinBox_ch4Brightness->value();
    lp.use_modbus = ui->checkBox_lightSControl->isChecked();

    ConfigManager::getInstance().saveConfig();
    SPDLOG_INFO("IO params saved");
}

void IOParamDialog::on_pushButton_setParam_clicked()
{
    saveParam();
    writeLuminance();
}

void IOParamDialog::slot_updateIODisplay()
{
    if (!isVisible())
        return;

    auto di = CommManager::getInstance().diState();
    auto do_ = CommManager::getInstance().doState();

    for (int i = 0; i < 8; ++i)
    {
        di_checkboxes_[i]->setChecked(di[i]);
        do_checkboxes_[i]->setChecked(do_[i]);
    }
}

void IOParamDialog::slot_doCheckBoxClicked(int index, bool checked)
{
    auto &mgr = CommManager::getInstance();
    uint16_t addr = ModbusAddr::DO0_VISION_OK + index;
    QMetaObject::invokeMethod(&mgr, [&mgr, addr, checked]()
                              { mgr.writeSingleCoil("plc_1", addr, checked); }, Qt::QueuedConnection);
}

void IOParamDialog::writeLuminance()
{
    auto &lp = AppContext::getInstance().light_param;
    auto &mgr = CommManager::getInstance();

    if (lp.use_modbus)
    {
        // Modbus 方式：写 4x 寄存器
        std::vector<uint16_t> values = {
            static_cast<uint16_t>(lp.luminance[0]),
            static_cast<uint16_t>(lp.luminance[1]),
            static_cast<uint16_t>(lp.luminance[2]),
            static_cast<uint16_t>(lp.luminance[3]),
        };
        QMetaObject::invokeMethod(&mgr, [&mgr, values]()
                                  { mgr.writeMultipleRegisters("plc_1", 0, values); }, Qt::QueuedConnection);
    }
    else
    {
        // 串口方式：直接发送光源控制器指令
        auto port = lp.serial_port;
        auto baud = lp.baud_rate;
        int lum[4] = {lp.luminance[0], lp.luminance[1], lp.luminance[2], lp.luminance[3]};
        QMetaObject::invokeMethod(&mgr, [&mgr, port, baud, lum]()
                                  {
            mgr.openLightSerial(port, baud);
            for (int ch = 1; ch <= 4; ++ch)
                mgr.setLuminance(ch, lum[ch - 1]); }, Qt::QueuedConnection);
    }
}
