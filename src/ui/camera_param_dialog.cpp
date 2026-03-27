#include "camera_param_dialog.h"
#include "ui_camera_param_dialog.h"
#include "../camera/camera_manager.h"
#include <spdlog/spdlog.h>
CameraParamDialog::CameraParamDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::CameraParamDialog)
{
    ui->setupUi(this);
}

CameraParamDialog::~CameraParamDialog()
{
    delete ui;
}

void CameraParamDialog::on_pushButton_setCameraParam_clicked()
{
    auto camera_gain = ui->doubleSpinBox_cameraGain->value();
    auto conductor_exposure = ui->spinBox_conductorExposure->value();
    // auto terminal_exposure = ui->spinBox_terminalExposure->value();
    // auto rubberCasing_exposure = ui->spinBox_rubberCasingExposure->value();
    // auto ouput_delay = ui->spinBox_outputDelay->value();
    // auto image_rotaAngle = ui->doubleSpinBox_imageRotaAngle->value();

    auto &mgr = CameraManager::getInstance();
    auto ids = mgr.cameraNames();
    if (ids.empty())
        return;
    auto *cam = mgr.getCamera(ids.front());
    if (!cam)
        return;
    cam->setGain(static_cast<float>(camera_gain));
    cam->setExposureTime(static_cast<float>(conductor_exposure));
}

void CameraParamDialog::on_pushButton_getCameraParam_clicked()
{
    auto &mgr = CameraManager::getInstance();
    auto ids = mgr.cameraNames();
    if (ids.empty())
        return;
    auto *cam = mgr.getCamera(ids.front());
    if (!cam)
        return;

    float camera_gain = 0.0f, us = 0.0f;
    if (!cam->getGain(camera_gain))
        SPDLOG_WARN("Failed to get gain from camera {}", ids.front());
    if (!cam->getExposureTime(us))
        SPDLOG_WARN("Failed to get exposure from camera {}", ids.front());
    ui->doubleSpinBox_cameraGain->setValue(static_cast<double>(camera_gain));
    ui->spinBox_conductorExposure->setValue(static_cast<int>(us));
}
