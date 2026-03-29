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

    ioparam_dialog_ = new IOParamDialog(this);
    camera_param_dialog_ = new CameraParamDialog(this);

    // 右侧面板：工作流配置 + 操作面板
    workflow_view_widget_ = new WorkflowViewWidget(ui->widget_region_operation);
    operation_view_widget_ = new OperationViewWidget(ui->widget_region_operation);

    auto *op_layout = new QVBoxLayout(ui->widget_region_operation);
    op_layout->setContentsMargins(0, 0, 0, 0);
    op_layout->setSpacing(0);
    op_layout->addWidget(workflow_view_widget_);
    op_layout->addWidget(operation_view_widget_, 1);

    workflow_view_widget_->setCameraViewFinder([this](const std::string &name) -> CameraViewWidget *
                                               {
        for (auto *v : camera_view_list_)
            if (v->cameraName() == name) return v;
        return nullptr; });

    // 用户选中某条 workflow → 更新 selectedWorkflows 缓存
    connect(workflow_view_widget_, &WorkflowViewWidget::workflowSelected, this,
            [](const std::string &camera_name, const std::string &workflow_name)
            {
                AppContext::getInstance().selected_workflows[camera_name] = workflow_name;
                SPDLOG_INFO("Selected workflow: camera={} wf={}", camera_name, workflow_name);
            });

    // 用户在 Dialog 中修改参数 → 重建对应 Pipeline
    connect(workflow_view_widget_, &WorkflowViewWidget::workflowConfigChanged,
            this, [](const std::string &wf_name)
            { WorkflowManager::getInstance().rebuildWorkflow(wf_name); });

    // rebuild 失败（workflow 忙碌）→ 提示用户
    connect(&WorkflowManager::getInstance(), &WorkflowManager::sign_rebuildFailed,
            this, [this](const std::string &wf_name)
            { QMessageBox::warning(this, QStringLiteral("配置未生效"),
                                   QStringLiteral("工作流 \"%1\" 正在运行中，参数将在本次检测完成后生效。\n如需立即生效请停止自动检测后重试。")
                                       .arg(QString::fromStdString(wf_name))); });

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
    for (auto &[name, _] : AppContext::getInstance().camera_params)
        addCameraUI(name);

    layoutCameraViews();

    if (!camera_view_list_.empty())
    {
        const auto &name = camera_view_list_.front()->cameraName();
        AppContext::getInstance().selected_camera_name = name;
        workflow_view_widget_->loadWorkflow(name);
    }
}

void MainWindow::addCameraUI(const std::string &cam_name)
{
    for (auto *v : camera_view_list_)
        if (v->cameraName() == cam_name)
            return;

    auto *view = new CameraViewWidget(cam_name, ui->widget_region_camera);
    camera_view_list_.push_back(view);

    connect(view, &CameraViewWidget::maximizeRequested, this,
            [this](const std::string &name)
            {
                if (maximized_camera_name_.empty())
                {
                    maximized_camera_name_ = name;
                    for (auto *v : camera_view_list_)
                        if (v->cameraName() != name)
                            v->hide();
                }
                else
                {
                    maximized_camera_name_.clear();
                    for (auto *v : camera_view_list_)
                        v->show();
                }
            });

    connect(view, &CameraViewWidget::selected, this,
            [this](const std::string &name)
            {
                AppContext::getInstance().selected_camera_name = name;
                workflow_view_widget_->loadWorkflow(name);
                auto it = AppContext::getInstance().selected_workflows.find(name);
                if (it != AppContext::getInstance().selected_workflows.end())
                    workflow_view_widget_->setSelectedWorkflow(it->second);
            });

    connect(view, &CameraViewWidget::cameraError, this,
            [](const std::string &name, int /*code*/)
            { CameraManager::getInstance().markOffline(name); });

    // 帧直接投递到 WorkflowManager，不经过 MainWindow
    connect(view, &CameraViewWidget::frameArrived,
            &WorkflowManager::getInstance(), &WorkflowManager::onFrameArrived);

    auto *cam = CameraManager::getInstance().getCamera(cam_name);
    bool online = cam && cam->isOpened();
    if (cam)
        cam->setCallback(view);
    view->setStatus(online ? QStringLiteral("在线") : QStringLiteral("离线"));

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
                for (auto *v : camera_view_list_)
                    if (v->cameraName() == camera_name)
                    {
                        v->updateFrame(image);
                        break;
                    }
            });

    // 检测完成 → 显示叠加结果图
    connect(&wfm, &WorkflowManager::sign_inspectionFinished, this,
            [this](const std::string &, const std::string &camera_name,
                   const HalconCpp::HObject &display_image, const InspectionResult &result)
            {
                for (auto *v : camera_view_list_)
                    if (v->cameraName() == camera_name)
                    {
                        v->updateFrame(display_image);
                        break;
                    }
                SPDLOG_INFO("Inspection: camera={} pass={}", camera_name, result.pass);
            });

    wfm.startAll();
}

void MainWindow::initStatusBar()
{
    auto &comm_mgr = CommManager::getInstance();
    for (auto &[name, _] : AppContext::getInstance().comm_params)
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
            [this](const std::string &name, bool connected)
            { setCommLed(name, connected); });
    connect(&comm_mgr, &CommManager::sign_lightStatusChanged, this,
            [this](bool connected)
            { setLightLed(connected); });
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
        auto sel_it = AppContext::getInstance().selected_workflows.find(AppContext::getInstance().selected_camera_name);
        if (sel_it == AppContext::getInstance().selected_workflows.end())
        {
            // 没有选中 workflow，只显示图像
            for (auto *v : camera_view_list_)
                if (v->cameraName() == AppContext::getInstance().selected_camera_name)
                {
                    v->updateFrame(image);
                    break;
                }
            return;
        }

        const std::string &wf_name = sel_it->second;

        // 更新画面
        auto &wfParams = AppContext::getInstance().workflow_params;
        auto wf_it = wfParams.find(wf_name);
        if (wf_it != wfParams.end())
        {
            const auto &cname = wf_it->second.camera_name;
            for (auto *v : camera_view_list_)
                if (v->cameraName() == cname)
                {
                    v->updateFrame(image);
                    break;
                }
        }

        // 注入帧并触发检测
        auto &wfm = WorkflowManager::getInstance();
        wfm.onFrameArrived(AppContext::getInstance().selected_camera_name, image); // 更新帧缓存
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
    for (auto &[name, comm_cfg] : AppContext::getInstance().comm_params)
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
