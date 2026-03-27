#include "main_window.h"
#include "ui_main_window.h"
#include "camera_view_widget.h"
#include "ioparam_dialog.h"
#include "camera_param_dialog.h"
#include "toolbox_widget.h"
#include "workflow_view_widget.h"
#include "operation_view_widget.h"
#include "../app/common.h"
#include "../app/app_context.h"
#include "../app/config_manager.h"
#include "../camera/camera_manager.h"
#include "../communication/comm_manager.h"
#include "../workflow/workflow_manager.h"
#include <spdlog/spdlog.h>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <halconcpp/HalconCpp.h>

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

    // 右侧面板：工具箱（全局）+ 工作流编辑器（按相机切换）
    toolbox_widget_ = new ToolboxWidget(ui->widget_region_operation);
    workflow_view_widget_ = new WorkflowViewWidget(toolbox_widget_, ui->widget_region_operation);
    operation_view_widget_ = new OperationViewWidget(ui->widget_region_operation);

    auto *op_layout = new QVBoxLayout(ui->widget_region_operation);
    op_layout->setContentsMargins(0, 0, 0, 0);
    op_layout->setSpacing(0);
    op_layout->addWidget(toolbox_widget_);
    op_layout->addWidget(workflow_view_widget_, 1);
    op_layout->addWidget(operation_view_widget_, 2);

    // 提供 CameraViewWidget 查找回调
    workflow_view_widget_->setCameraViewFinder([this](const std::string &name) -> CameraViewWidget *
                                               {
        auto it = camera_views_.find(name);
        return it != camera_views_.end() ? it->second : nullptr; });

    // 工具箱双击 → 添加算法到当前相机的流程
    connect(toolbox_widget_, &ToolboxWidget::algorithmActivated,
            workflow_view_widget_, &WorkflowViewWidget::addAlgorithm);

    connect(workflow_view_widget_, &WorkflowViewWidget::workflowChanged,
            this, [](const std::string &camera_name, const std::vector<std::string> &)
            { WorkflowManager::getInstance().rebuildWorkflow(camera_name); });

    // 连接相机管理器的状态变化信号
    auto &mgr = CameraManager::getInstance();
    connect(&mgr, &CameraManager::sign_cameraStatusChanged, this, [this](const std::string &cam_name, bool online)
            {
        // 如果是配置之外新发现的相机，动态创建 view 和 LED
        if (camera_views_.find(cam_name) == camera_views_.end())
        {
            addCameraUI(cam_name);
        }

        // 上线时绑定回调（确保热插拔/重连后也能收到帧）
        if (online)
        {
            auto *cam = CameraManager::getInstance().getCamera(cam_name);
            if (cam)
                cam->setCallback(this);
        }

        // 更新状态文字
        auto it = camera_views_.find(cam_name);
        if (it != camera_views_.end())
        {
            it->second->setStatus(online ? QStringLiteral("在线") : QStringLiteral("离线"));
        }
        setCameraLed(cam_name, online);
        if (online) SPDLOG_INFO("Camera {} online", cam_name); });

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
    for (auto &[name, cam_cfg] : AppContext::getInstance().cameraParams())
    {
        addCameraUI(name);
    }

    // 为已在线的相机绑定回调
    auto &mgr = CameraManager::getInstance();
    for (auto &cam_name : mgr.cameraNames())
    {
        auto *cam = mgr.getCamera(cam_name);
        if (cam)
            cam->setCallback(this);
    }

    // 按 2x2 田字格布局
    layoutCameraViews();

    // 默认选中第一个相机
    if (!camera_view_list_.empty())
        slot_cameraSelected(camera_view_list_.front()->cameraName());
}

void MainWindow::addCameraUI(const std::string &cam_name)
{
    if (camera_views_.count(cam_name))
        return; // 已存在

    auto *view = new CameraViewWidget(cam_name, ui->widget_region_camera);
    camera_views_[cam_name] = view;
    camera_view_list_.push_back(view);
    connect(view, &CameraViewWidget::maximizeRequested,
            this, &MainWindow::slot_cameraMaximizeRequested);
    connect(view, &CameraViewWidget::selected,
            this, &MainWindow::slot_cameraSelected);

    // 检查相机是否已在线
    auto *cam = CameraManager::getInstance().getCamera(cam_name);
    bool online = cam && cam->isOpened();
    if (cam)
        cam->setCallback(this);

    view->setStatus(online ? QStringLiteral("在线") : QStringLiteral("离线"));

    // 动态添加状态灯
    if (camera_status_leds_.find(cam_name) == camera_status_leds_.end())
    {
        ui->statusbar->addPermanentWidget(new QLabel(QString::fromStdString(cam_name), this));
        auto *led = new QLabel(this);
        led->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
        ui->statusbar->addPermanentWidget(led);
        camera_status_leds_[cam_name] = led;
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
    for (auto &[name, comm_cfg] : AppContext::getInstance().commParams())
    {
        ui->statusbar->addPermanentWidget(new QLabel(QString::fromStdString(name), this));
        auto *led = new QLabel(this);
        led->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
        ui->statusbar->addPermanentWidget(led);
        comm_status_leds_[name] = led;
    }

    // 光源状态灯
    ui->statusbar->addPermanentWidget(new QLabel(QStringLiteral("光源"), this));
    light_status_led_ = new QLabel(this);
    light_status_led_->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->statusbar->addPermanentWidget(light_status_led_);

    // 连接通信状态变化信号
    connect(&comm_mgr, &CommManager::sign_commStatusChanged, this,
            [this](const std::string &name, bool connected)
            {
                setCommLed(name, connected);
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

void MainWindow::on_action_open_folder_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择图片文件夹"));
    if (dir.isEmpty())
        return;

    QDir folder(dir);
    QStringList filters = {"*.bmp", "*.png", "*.jpg", "*.jpeg", "*.tif", "*.tiff"};
    auto entries = folder.entryInfoList(filters, QDir::Files, QDir::Name);
    if (entries.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("文件夹中没有找到图片"));
        return;
    }

    offline_image_paths_.clear();
    for (auto &fi : entries)
        offline_image_paths_.push_back(fi.absoluteFilePath());
    offline_image_index_ = 0;

    SPDLOG_INFO("Loaded {} offline images from {}", offline_image_paths_.size(), dir.toStdString());

    // 执行第一张
    runOfflineTest(offline_image_paths_[0]);
}

void MainWindow::runOfflineTest(const QString &image_path)
{
    try
    {
        HalconCpp::HObject image;
        HalconCpp::ReadImage(&image, image_path.toStdString().c_str());

        // 显示到第一个相机 view
        auto &wfParams = AppContext::getInstance().workflowParams();
        if (wfParams.empty())
        {
            // 没有工作流配置，直接显示到第一个 camera view
            if (!camera_view_list_.empty())
                camera_view_list_.front()->updateFrame(image);
            return;
        }

        // 使用第一个工作流执行离线检测
        auto &[cam_name, wf] = *wfParams.begin();
        auto it = camera_views_.find(wf.camera_name);
        if (it != camera_views_.end())
            it->second->updateFrame(image);

        auto &wfm = WorkflowManager::getInstance();
        wfm.setOfflineImage(wf.name, image);
        wfm.triggerOnce(wf.name);

        SPDLOG_INFO("Offline test: [{}/{}] {}",
                    offline_image_index_ + 1, offline_image_paths_.size(),
                    image_path.toStdString());
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Failed to read image {}: {}", image_path.toStdString(), e.ErrorMessage().Text());
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法读取图片: %1").arg(image_path));
    }
}

void MainWindow::slot_cameraMaximizeRequested(const std::string &camera_name)
{
    if (maximized_camera_name_.empty())
    {
        // 放大：隐藏其他，让当前 widget 撑满
        maximized_camera_name_ = camera_name;
        for (auto *view : camera_view_list_)
        {
            if (view->cameraName() != camera_name)
                view->hide();
        }
    }
    else
    {
        // 还原：显示所有
        maximized_camera_name_.clear();
        for (auto *view : camera_view_list_)
        {
            view->show();
        }
    }
}

void MainWindow::slot_cameraSelected(const std::string &camera_name)
{
    selected_camera_name_ = camera_name;

    // 右侧面板显示该相机的工作流
    workflow_view_widget_->loadWorkflow(camera_name);
}

void MainWindow::frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame)
{
    // SDK 回调线程 → 投递到主线程刷新 UI
    HalconCpp::HObject image_copy = frame;
    QMetaObject::invokeMethod(this, [this, cam_name = camera_name, img = std::move(image_copy)]()
                              {
        auto it = camera_views_.find(cam_name);
        if (it != camera_views_.end())
        {
            it->second->updateFrame(img);
        } }, Qt::QueuedConnection);
}

void MainWindow::setCameraLed(const std::string &camera_name, bool online)
{
    auto it = camera_status_leds_.find(camera_name);
    if (it != camera_status_leds_.end())
    {
        it->second->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
    }
}

void MainWindow::setCommLed(const std::string &comm_name, bool connected)
{
    auto it = comm_status_leds_.find(comm_name);
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
    for (auto &[name, comm_cfg] : AppContext::getInstance().commParams())
    {
        QMetaObject::invokeMethod(&mgr, [&mgr, comm_cfg]()
                                  { mgr.addComm(comm_cfg); }, Qt::QueuedConnection);
    }
}

void MainWindow::initWorkflow()
{
    // 确保每个相机都有对应的 WorkflowParam
    auto &ctx = AppContext::getInstance();
    for (auto &[cam_name, cam] : ctx.cameraParams())
    {
        if (ctx.workflowParams().count(cam_name) == 0)
        {
            WorkflowParam wp;
            wp.name = "wf_" + cam_name;
            wp.camera_name = cam_name;
            ctx.workflowParams()[cam_name] = wp;
        }
    }

    auto &wfm = WorkflowManager::getInstance();
    wfm.buildAll();

    // 采集完成 → 显示原图
    connect(&wfm, &WorkflowManager::sign_frameCaptured, this,
            [this](const std::string &camera_name, const HalconCpp::HObject &image)
            {
                auto it = camera_views_.find(camera_name);
                if (it != camera_views_.end())
                    it->second->updateFrame(image);
            });

    // 检测完成 → 显示叠加结果图
    connect(&wfm, &WorkflowManager::sign_inspectionFinished, this,
            [this](const std::string &workflow_name, const std::string &camera_name,
                   const HalconCpp::HObject &display_image, const InspectionResult &result)
            {
                auto it = camera_views_.find(camera_name);
                if (it != camera_views_.end())
                    it->second->updateFrame(display_image);
                SPDLOG_INFO("Inspection finished: workflow={} pass={}", workflow_name, result.pass);
            });

    // 自动开始（对应 C# Form1_Load 末尾 btStartAuto.PerformClick()）
    wfm.startAll();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (offline_image_paths_.size() <= 1)
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    bool changed = false;
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down)
    {
        if (offline_image_index_ + 1 < static_cast<int>(offline_image_paths_.size()))
        {
            offline_image_index_++;
            changed = true;
        }
    }
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up)
    {
        if (offline_image_index_ > 0)
        {
            offline_image_index_--;
            changed = true;
        }
    }

    if (changed)
        runOfflineTest(offline_image_paths_[offline_image_index_]);
    else
        QMainWindow::keyPressEvent(event);
}

void MainWindow::cameraErrorReceived(const std::string &camera_name, int error_code, const std::string &msg)
{
    SPDLOG_ERROR("Camera {} error 0x{:08X}: {}", camera_name, error_code, msg);

    // SDK回调线程 → 投递到主线程，触发重连（markOffline 会 emit cameraOffline 信号更新 UI）
    QMetaObject::invokeMethod(this, [camera_name]()
                              { CameraManager::getInstance().markOffline(camera_name); }, Qt::QueuedConnection);
}
