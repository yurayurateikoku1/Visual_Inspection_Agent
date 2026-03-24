#include "main_window.h"
#include "ui_main_window.h"
#include "camera_view_widget.h"
#include "param_page.h"
#include "../app/config_manager.h"
#include "../camera/camera_manager.h"
#include "../camera/hik_camera.h"
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

        // 为相机区域创建网格布局
    camera_layout_ = new QGridLayout(ui->widget_region_camera);
    camera_layout_->setContentsMargins(4, 4, 4, 4);
    camera_layout_->setSpacing(4);

    param_page_ = new ParamPage(this);

    initCameras();
}

void MainWindow::on_action_parameter_triggered()
{
    param_page_->exec();
}

MainWindow::~MainWindow()
{
    CameraManager::getInstance().closeAll();
    delete ui;
}

void MainWindow::initCameras()
{
    // 1. 枚举所有在线海康设备，自动生成配置
    auto devices = HikCamera::enumDevices();
    if (devices.empty())
    {
        SPDLOG_WARN("No HIK camera devices found");
        return;
    }
    SPDLOG_INFO("Found {} HIK devices", devices.size());

    auto &mgr = CameraManager::getInstance();

    for (size_t i = 0; i < devices.size(); ++i)
    {
        auto &dev = devices[i];

        // 从设备信息构造 CameraConfig
        CameraConfig cfg;
        cfg.id = "cam_" + std::to_string(i + 1);

        if (dev.nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = dev.SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            cfg.ip = ip;
            // 用用户自定义名称，若为空则用 IP
            cfg.name = (gige.chUserDefinedName[0] != '\0')
                           ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                           : cfg.ip;
        }
        else if (dev.nTLayerType == MV_USB_DEVICE)
        {
            auto &usb = dev.SpecialInfo.stUsb3VInfo;
            cfg.name = (usb.chUserDefinedName[0] != '\0')
                           ? std::string(reinterpret_cast<const char *>(usb.chUserDefinedName))
                           : std::string(reinterpret_cast<const char *>(usb.chSerialNumber));
        }

        // 2. 创建 CameraViewWidget
        auto *view = new CameraViewWidget(cfg.id, ui->widget_region_camera);
        camera_views_[cfg.id] = view;
        camera_view_list_.push_back(view);
        connect(view, &CameraViewWidget::maximizeRequested,
                this, &MainWindow::onCameraMaximizeRequested);

        // 3. 直接用设备信息打开相机（免二次枚举）
        if (mgr.addCamera(cfg, &dev))
        {
            auto *cam = mgr.getCamera(cfg.id);
            cam->setCallback(this);
            cam->startGrabbing();
            view->setStatus(QString::fromStdString(cfg.name + " [在线]"));
            SPDLOG_INFO("Camera {} ({}) started", cfg.id, cfg.name);
        }
        else
        {
            view->setStatus(QString::fromStdString(cfg.name + " [打开失败]"));
        }
    }

    // 按 2x2 田字格布局
    layoutCameraViews();
}

void MainWindow::layoutCameraViews()
{
    // 先移除布局中所有 widget（不删除）
    while (camera_layout_->count() > 0)
    {
        camera_layout_->takeAt(0);
    }

    // 固定 2x2 田字格：(0,0) (0,1) (1,0) (1,1)
    for (size_t i = 0; i < camera_view_list_.size() && i < 4; ++i)
    {
        int row = static_cast<int>(i) / 2;
        int col = static_cast<int>(i) % 2;
        camera_layout_->addWidget(camera_view_list_[i], row, col);
        camera_view_list_[i]->show();
    }
}

void MainWindow::onCameraMaximizeRequested(const std::string &camera_id)
{
    if (maximized_camera_id_.empty())
    {
        // 放大：隐藏其他，让当前 widget 撑满
        maximized_camera_id_ = camera_id;
        for (auto *view : camera_view_list_)
        {
            if (view->cameraId() != camera_id)
                view->hide();
        }
    }
    else
    {
        // 还原：显示所有
        maximized_camera_id_.clear();
        for (auto *view : camera_view_list_)
        {
            view->show();
        }
    }
}

void MainWindow::onFrameReceived(const std::string &camera_id, const cv::Mat &frame)
{
    auto it = camera_views_.find(camera_id);
    if (it != camera_views_.end())
    {
        it->second->updateFrame(frame);
    }
}

void MainWindow::onCameraError(const std::string &camera_id, int error_code, const std::string &msg)
{
    SPDLOG_ERROR("Camera {} error 0x{:08X}: {}", camera_id, error_code, msg);
    auto it = camera_views_.find(camera_id);
    if (it != camera_views_.end())
    {
        it->second->setStatus(QString::fromStdString(camera_id + " [错误]"));
    }
}
