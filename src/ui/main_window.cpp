#include "main_window.h"
#include "ui_main_window.h"
#include "camera_view_widget.h"
#include "ioparam_dialog.h"
#include "camera_param_dialog.h"
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

    camera_layout_ = new QGridLayout(ui->widget_region_camera);
    camera_layout_->setContentsMargins(4, 4, 4, 4);
    camera_layout_->setSpacing(4);

    ioparam_dialog_      = new IOParamDialog(this);
    camera_param_dialog_ = new CameraParamDialog(this);

    // 右侧面板：工作流配置 + 操作面板
    workflow_view_widget_   = new WorkflowViewWidget(ui->widget_region_operation);
    operation_view_widget_  = new OperationViewWidget(ui->widget_region_operation);

    auto *op_layout = new QVBoxLayout(ui->widget_region_operation);
    op_layout->setContentsMargins(0, 0, 0, 0);
    op_layout->setSpacing(0);
    op_layout->addWidget(workflow_view_widget_);
    op_layout->addWidget(operation_view_widget_, 1);

    workflow_view_widget_->setCameraViewFinder([this](const std::string &name) -> CameraViewWidget *
    {
        auto it = camera_views_.find(name);
        return it != camera_views_.end() ? it->second : nullptr;
    });

    // 用户选中某条 workflow → 更新 selected_workflow_ 缓存
    connect(workflow_view_widget_, &WorkflowViewWidget::workflowSelected,
            this, &MainWindow::slot_onWorkflowSelected);

    // 用户在 Dialog 中修改参数 → 重建对应 Pipeline
    connect(workflow_view_widget_, &WorkflowViewWidget::workflowConfigChanged,
            this, [](const std::string &wf_name)
            { WorkflowManager::getInstance().rebuildWorkflow(wf_name); });

    // rebuild 失败（workflow 忙碌）→ 提示用户
    connect(&WorkflowManager::getInstance(), &WorkflowManager::sign_rebuildFailed,
            this, [this](const std::string &wf_name)
            {
                QMessageBox::warning(this, QStringLiteral("配置未生效"),
                    QStringLiteral("工作流 "%1" 正在运行中，参数将在本次检测完成后生效。\n如需立即生效请停止自动检测后重试。")
                        .arg(QString::fromStdString(wf_name)));
            });

    // 相机热插拔状态变化
    auto &mgr = CameraManager::getInstance();
    connect(&mgr, &CameraManager::sign_cameraStatusChanged, this,
            [this](const std::string &cam_name, bool online)
            {
                if (camera_views_.find(cam_name) == camera_views_.end())
                    addCameraUI(cam_name);

                if (online)
                {
                    auto *cam = CameraManager::getInstance().getCamera(cam_name);
                    auto vit  = camera_views_.find(cam_name);
                    if (cam && vit != camera_views_.end())
                        cam->setCallback(vit->second);
                }

                auto it = camera_views_.find(cam_name);
                if (it != camera_views_.end())
                    it->second->setStatus(online ? QStringLiteral("在线") : QStringLiteral("离线"));

                setCameraLed(cam_name, online);
                if (online) SPDLOG_INFO("Camera {} online", cam_name);
            });

    initCameras();
    initModbusCommunication();
    initWorkflow();
    initStatusBar();
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
    for (auto &[name, _] : AppContext::getInstance().cameraParams())
        addCameraUI(name);

    auto &mgr = CameraManager::getInstance();
    for (auto &cam_name : mgr.cameraNames())
    {
        auto *cam = mgr.getCamera(cam_name);
        auto vit  = camera_views_.find(cam_name);
        if (cam && vit != camera_views_.end())
            cam->setCallback(vit->second);
    }

    layoutCameraViews();

    if (!camera_view_list_.empty())
        slot_cameraSelected(camera_view_list_.front()->cameraName());
}

void MainWindow::addCameraUI(const std::string &cam_name)
{
    if (camera_views_.count(cam_name))
        return;

    auto *view = new CameraViewWidget(cam_name, ui->widget_region_camera);
    camera_views_[cam_name] = view;
    camera_view_list_.push_back(view);

    connect(view, &CameraViewWidget::maximizeRequested,
            this, &MainWindow::slot_cameraMaximizeRequested);
    connect(view, &CameraViewWidget::selected,
            this, &MainWindow::slot_cameraSelected);
    connect(view, &CameraViewWidget::cameraError,
            this, &MainWindow::slot_onCameraError);

    // 帧直接投递到 WorkflowManager，不经过 MainWindow
    connect(view, &CameraViewWidget::frameArrived,
            &WorkflowManager::getInstance(), &WorkflowManager::onFrameArrived);

    auto *cam   = CameraManager::getInstance().getCamera(cam_name);
    bool online = cam && cam->isOpened();
    if (cam)
        cam->setCallback(view);

    view->setStatus(online ? QStringLiteral("在线") : QStringLiteral("离线"));

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
    while (camera_layout_->count() > 0)
        camera_layout_->takeAt(0);

    for (size_t i = 0; i < camera_view_list_.size() && i < 4; ++i)
    {
        int row = static_cast<int>(i) / 2;
        int col = static_cast<int>(i) % 2;
        camera_layout_->addWidget(camera_view_list_[i], row, col);
        camera_view_list_[i]->show();
    }
}

void MainWindow::initWorkflow()
{
    // 确保每个相机都有4条 WorkflowParam（DI0~DI3）
    AppContext::getInstance().ensureWorkflowsForAllCameras();

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
            [this](const std::string &, const std::string &camera_name,
                   const HalconCpp::HObject &display_image, const InspectionResult &result)
            {
                auto it = camera_views_.find(camera_name);
                if (it != camera_views_.end())
                    it->second->updateFrame(display_image);
                SPDLOG_INFO("Inspection: camera={} pass={}", camera_name, result.pass);
            });

    wfm.startAll();
}

void MainWindow::initStatusBar()
{
    auto &comm_mgr = CommManager::getInstance();
    for (auto &[name, _] : AppContext::getInstance().commParams())
    {
        ui->statusbar->addPermanentWidget(new QLabel(QString::fromStdString(name), this));
        auto *led = new QLabel(this);
        led->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
        ui->statusbar->addPermanentWidget(led);
        comm_status_leds_[name] = led;
    }

    ui->statusbar->addPermanentWidget(new QLabel(QStringLiteral("光源"), this));
    light_status_led_ = new QLabel(this);
    light_status_led_->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->statusbar->addPermanentWidget(light_status_led_);

    connect(&comm_mgr, &CommManager::sign_commStatusChanged, this,
            [this](const std::string &name, bool connected) { setCommLed(name, connected); });
    connect(&comm_mgr, &CommManager::sign_lightStatusChanged, this,
            [this](bool connected) { setLightLed(connected); });
}

void MainWindow::on_action_parameter_triggered()
{
    ioparam_dialog_->exec();
}

void MainWindow::on_action_camera_triggered()
{
    camera_param_dialog_->exec();
}

void MainWindow::on_action_open_folder_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图片文件夹"));
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
    runOfflineTest(offline_image_paths_[0]);
}

void MainWindow::runOfflineTest(const QString &image_path)
{
    try
    {
        HalconCpp::HObject image;
        HalconCpp::ReadImage(&image, image_path.toStdString().c_str());

        // 找当前相机选中的 workflow
        auto sel_it = selected_workflow_.find(selected_camera_name_);
        if (sel_it == selected_workflow_.end())
        {
            // 没有选中 workflow，只显示图像
            auto vit = camera_views_.find(selected_camera_name_);
            if (vit != camera_views_.end())
                vit->second->updateFrame(image);
            return;
        }

        const std::string &wf_name = sel_it->second;

        // 更新画面
        auto &wfParams = AppContext::getInstance().workflowParams();
        auto wf_it = wfParams.find(wf_name);
        if (wf_it != wfParams.end())
        {
            auto vit = camera_views_.find(wf_it->second.camera_name);
            if (vit != camera_views_.end())
                vit->second->updateFrame(image);
        }

        // 注入帧并触发检测
        auto &wfm = WorkflowManager::getInstance();
        wfm.onFrameArrived(selected_camera_name_, image); // 更新帧缓存
        wfm.triggerOnce(wf_name);

        SPDLOG_INFO("Offline test [{}/{}]: {} → {}",
                    offline_image_index_ + 1, offline_image_paths_.size(),
                    image_path.toStdString(), wf_name);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Failed to read image {}: {}", image_path.toStdString(), e.ErrorMessage().Text());
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法读取图片: %1").arg(image_path));
    }
}

void MainWindow::slot_cameraSelected(const std::string &camera_name)
{
    selected_camera_name_ = camera_name;
    workflow_view_widget_->loadWorkflow(camera_name);

    // 恢复该相机上次选中的 workflow 高亮
    auto it = selected_workflow_.find(camera_name);
    if (it != selected_workflow_.end())
        workflow_view_widget_->setSelectedWorkflow(it->second);
}

void MainWindow::slot_onWorkflowSelected(const std::string &camera_name,
                                          const std::string &workflow_name)
{
    selected_workflow_[camera_name] = workflow_name;
    SPDLOG_INFO("Selected workflow: camera={} wf={}", camera_name, workflow_name);
}

void MainWindow::slot_cameraMaximizeRequested(const std::string &camera_name)
{
    if (maximized_camera_name_.empty())
    {
        maximized_camera_name_ = camera_name;
        for (auto *view : camera_view_list_)
            if (view->cameraName() != camera_name)
                view->hide();
    }
    else
    {
        maximized_camera_name_.clear();
        for (auto *view : camera_view_list_)
            view->show();
    }
}

void MainWindow::slot_onCameraError(const std::string &camera_name, int /*error_code*/)
{
    CameraManager::getInstance().markOffline(camera_name);
}

void MainWindow::setCameraLed(const std::string &camera_name, bool online)
{
    auto it = camera_status_leds_.find(camera_name);
    if (it != camera_status_leds_.end())
        it->second->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
}

void MainWindow::setCommLed(const std::string &comm_name, bool connected)
{
    auto it = comm_status_leds_.find(comm_name);
    if (it != comm_status_leds_.end())
        it->second->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
}

void MainWindow::setLightLed(bool connected)
{
    if (light_status_led_)
        light_status_led_->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
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
