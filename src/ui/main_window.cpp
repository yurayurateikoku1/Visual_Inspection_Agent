#include "main_window.h"
#include "ui_main_window.h"
#include "camera_view_widget.h"
#include "ioparam_dialog.h"
#include "camera_param_dialog.h"
#include "../app/common.h"
#include "../app/app_context.h"
#include "../app/config_manager.h"
#include "../camera/camera_manager.h"
#include "../communication/comm_manager.h"
#include "../workflow/workflow_manager.h"
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 为相机区域创建网格布局
    camera_layout_ = new QGridLayout(ui->widget_region_camera);
    camera_layout_->setContentsMargins(4, 4, 4, 4);
    camera_layout_->setSpacing(4);

    ioparam_dialog_ = new IOParamDialog(this);
    camera_param_dialog_ = new CameraParamDialog(this);

    // 连接相机管理器的状态变化信号
    auto &mgr = CameraManager::getInstance();
    connect(&mgr, &CameraManager::sign_cameraStatusChanged, this, [this](const std::string &cam_id, bool online)
            {
        // 如果是配置之外新发现的相机，动态创建 view 和 LED
        if (camera_views_.find(cam_id) == camera_views_.end())
        {
            addCameraUI(cam_id);
        }

        // 上线时绑定回调（确保热插拔/重连后也能收到帧）
        if (online)
        {
            auto *cam = CameraManager::getInstance().getCamera(cam_id);
            if (cam)
                cam->setCallback(this);
        }

        // 更新状态文字
        auto it = camera_views_.find(cam_id);
        if (it != camera_views_.end())
        {
            auto *cam = CameraManager::getInstance().getCamera(cam_id);
            std::string name = (cam && !cam->getName().empty()) ? cam->getName() : cam_id;
            it->second->setStatus(QString::fromStdString(name + (online ? " [在线]" : " [离线]")));
        }
        setCameraLed(cam_id, online);
        if (online) SPDLOG_INFO("Camera {} online", cam_id); });

    initCameras();
    initModbusCommunication();
    initWorkflow();
    initStatusBar();
}

void MainWindow::on_action_parameter_triggered()
{
    ioparam_dialog_->exec();
}

MainWindow::~MainWindow()
{
    WorkflowManager::getInstance().stopAll();
    CommManager::getInstance().shutdown();
    CameraManager::getInstance().closeAll();
    delete ui;
}

void MainWindow::initCameras()
{
    // 根据配置预创建所有相机 view（无论相机是否在线）
    for (auto &cam_cfg : AppContext::getInstance().cameraParams())
    {
        addCameraUI(cam_cfg.id);
    }

    // 为已在线的相机绑定回调
    auto &mgr = CameraManager::getInstance();
    for (auto &cam_id : mgr.cameraIds())
    {
        auto *cam = mgr.getCamera(cam_id);
        if (cam)
            cam->setCallback(this);
    }

    // 按 2x2 田字格布局
    layoutCameraViews();
}

void MainWindow::addCameraUI(const std::string &cam_id)
{
    if (camera_views_.count(cam_id))
        return; // 已存在

    auto *view = new CameraViewWidget(cam_id, ui->widget_region_camera);
    camera_views_[cam_id] = view;
    camera_view_list_.push_back(view);
    connect(view, &CameraViewWidget::maximizeRequested,
            this, &MainWindow::slot_cameraMaximizeRequested);

    // 检查相机是否已在线
    auto *cam = CameraManager::getInstance().getCamera(cam_id);
    bool online = cam && cam->isOpened();
    if (cam)
        cam->setCallback(this);

    // 优先用配置中的名称
    std::string display_name = cam_id;
    for (auto &cfg : AppContext::getInstance().cameraParams())
    {
        if (cfg.id == cam_id && !cfg.name.empty())
        {
            display_name = cfg.name;
            break;
        }
    }
    view->setStatus(QString::fromStdString(
        display_name + (online ? " [在线]" : " [离线]")));

    // 动态添加状态灯
    if (camera_status_leds_.find(cam_id) == camera_status_leds_.end())
    {
        QString num = QString::fromStdString(cam_id).mid(4);
        ui->statusbar->addPermanentWidget(new QLabel(QStringLiteral("相机") + num, this));
        auto *led = new QLabel(this);
        led->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
        ui->statusbar->addPermanentWidget(led);
        camera_status_leds_[cam_id] = led;
    }

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

void MainWindow::initStatusBar()
{
    // 相机状态灯已在 initCameras() → addCameraUI() 中动态创建

    // Modbus 通信状态灯
    auto &comm_mgr = CommManager::getInstance();
    for (auto &comm_cfg : AppContext::getInstance().commParams())
    {
        ui->statusbar->addPermanentWidget(new QLabel(QString::fromStdString(comm_cfg.name), this));
        auto *led = new QLabel(this);
        led->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
        ui->statusbar->addPermanentWidget(led);
        comm_status_leds_[comm_cfg.id] = led;
    }

    // 光源状态灯
    ui->statusbar->addPermanentWidget(new QLabel(QStringLiteral("光源"), this));
    light_status_led_ = new QLabel(this);
    light_status_led_->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->statusbar->addPermanentWidget(light_status_led_);

    // 连接通信状态变化信号
    connect(&comm_mgr, &CommManager::sign_commStatusChanged, this,
            [this](const std::string &id, bool connected)
            {
                setCommLed(id, connected);
            });

    // 连接光源状态变化信号
    connect(&comm_mgr, &CommManager::sign_lightStatusChanged, this,
            [this](bool connected)
            {
                setLightLed(connected);
            });

    SPDLOG_INFO("initStatusBar successfully.");
}

void MainWindow::on_action_camera_triggered()
{
    camera_param_dialog_->exec();
}

void MainWindow::slot_cameraMaximizeRequested(const std::string &camera_id)
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

void MainWindow::onFrameReceived(const std::string &camera_id, const HalconCpp::HObject &frame)
{
    auto it = camera_views_.find(camera_id);
    if (it != camera_views_.end())
    {
        it->second->updateFrame(frame);
    }
}

void MainWindow::setCameraLed(const std::string &camera_id, bool online)
{
    auto it = camera_status_leds_.find(camera_id);
    if (it != camera_status_leds_.end())
    {
        it->second->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
    }
}

void MainWindow::setCommLed(const std::string &comm_id, bool connected)
{
    auto it = comm_status_leds_.find(comm_id);
    if (it != comm_status_leds_.end())
    {
        it->second->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
    }
}

void MainWindow::setLightLed(bool connected)
{
    if (light_status_led_)
    {
        light_status_led_->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
    }
}

void MainWindow::initModbusCommunication()
{
    auto &mgr = CommManager::getInstance();
    for (auto &comm_cfg : AppContext::getInstance().commParams())
    {
        QMetaObject::invokeMethod(&mgr, [&mgr, comm_cfg]()
                                  { mgr.addComm(comm_cfg); }, Qt::QueuedConnection);
    }
}

void MainWindow::initWorkflow()
{
    auto &wfm = WorkflowManager::getInstance();
    wfm.buildAll();

    // 采集完成 → 显示原图
    connect(&wfm, &WorkflowManager::sign_frameCaptured, this,
            [this](const std::string &camera_id, const HalconCpp::HObject &image)
            {
                auto it = camera_views_.find(camera_id);
                if (it != camera_views_.end())
                    it->second->updateFrame(image);
            });

    // 检测完成 → 显示叠加结果图
    connect(&wfm, &WorkflowManager::sign_inspectionFinished, this,
            [this](const std::string &workflow_id, const std::string &camera_id,
                   const HalconCpp::HObject &display_image, const InspectionResult &result)
            {
                auto it = camera_views_.find(camera_id);
                if (it != camera_views_.end())
                    it->second->updateFrame(display_image);
                SPDLOG_INFO("Inspection finished: workflow={} pass={}", workflow_id, result.pass);
            });

    // 自动开始（对应 C# Form1_Load 末尾 btStartAuto.PerformClick()）
    wfm.startAll();
}

void MainWindow::onCameraError(const std::string &camera_id, int error_code, const std::string &msg)
{
    SPDLOG_ERROR("Camera {} error 0x{:08X}: {}", camera_id, error_code, msg);

    // SDK回调线程 → 投递到主线程，触发重连（markOffline 会 emit cameraOffline 信号更新 UI）
    QMetaObject::invokeMethod(this, [camera_id]()
                              { CameraManager::getInstance().markOffline(camera_id); }, Qt::QueuedConnection);
}
