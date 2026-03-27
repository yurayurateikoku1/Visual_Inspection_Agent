#include "camera_view_widget.h"
#include "ui_camera_view_widget.h"
#include <QResizeEvent>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "../camera/camera_manager.h"

CameraViewWidget::CameraViewWidget(const std::string &camera_name, QWidget *parent)
    : QWidget(parent), ui(new Ui::CameraViewWidget), camera_name_(camera_name)
{
    ui->setupUi(this);

    ui->label_cameraId->setText(QString::fromStdString(camera_name));
    ui->label_status->setText(QStringLiteral("离线"));
    ui->pushButton_scaleWindow->setIcon(QIcon(":/assets/fangdachuangkou2x.png"));

    // Halcon 渲染画布
    ui->widget_window->setMinimumSize(320, 240);
    ui->widget_window->setStyleSheet("background-color: #1e1e1e;");
    ui->widget_window->setAttribute(Qt::WA_NativeWindow);
}

CameraViewWidget::~CameraViewWidget()
{
    hwindow_.reset();
    delete ui;
}

void CameraViewWidget::on_pushButton_workflow_clicked()
{
    emit selected(camera_name_);
}

void CameraViewWidget::on_pushButton_scaleWindow_clicked()
{
    maximized_ = !maximized_;
    ui->pushButton_scaleWindow->setIcon(maximized_ ? QIcon(":/assets/fangdachuangkou2x.png") : QIcon(":/assets/suoxiaochuangkou2x.png"));
    emit maximizeRequested(camera_name_);
}

void CameraViewWidget::initHalconWindow()
{
    if (hwindow_)
        return;

    // 使用物理像素尺寸（高DPI屏幕下逻辑像素 ≠ 物理像素）
    qreal dpr = ui->widget_window->devicePixelRatioF();
    int w = static_cast<int>(ui->widget_window->width() * dpr);
    int h = static_cast<int>(ui->widget_window->height() * dpr);
    if (w <= 0 || h <= 0)
        return;

    try
    {
        hwindow_ = std::make_unique<HalconCpp::HWindow>(
            0, 0, w - 1, h - 1,
            static_cast<Hlong>(ui->widget_window->winId()),
            "visible", "");
        hwindow_w_ = ui->widget_window->width();
        hwindow_h_ = ui->widget_window->height();
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("HWindow create failed: {}", e.ErrorMessage().Text());
    }
}

void CameraViewWidget::displayImage(const HalconCpp::HObject &image)
{
    int w = ui->widget_window->width();
    int h = ui->widget_window->height();

    // 窗口尺寸变化时重建 HWindow
    if (hwindow_ && (w != hwindow_w_ || h != hwindow_h_))
    {
        hwindow_.reset();
    }

    if (!hwindow_)
        initHalconWindow();
    if (!hwindow_)
        return;

    try
    {
        HalconCpp::HTuple img_w, img_h;
        HalconCpp::GetImageSize(image, &img_w, &img_h);
        hwindow_->SetPart(0, 0, img_h.I() - 1, img_w.I() - 1);
        hwindow_->DispObj(image);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("DispObj failed: {}", e.ErrorMessage().Text());
    }
}

void CameraViewWidget::frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame)
{
    // SDK 回调线程 → 投递到主线程
    HalconCpp::HObject image_copy = frame;
    QMetaObject::invokeMethod(this, [this, cam_name = camera_name, img = std::move(image_copy)]()
    {
        updateFrame(img);
        emit frameArrived(cam_name, img);
    }, Qt::QueuedConnection);
}

void CameraViewWidget::cameraErrorReceived(const std::string &camera_name, int error_code, const std::string &msg)
{
    SPDLOG_ERROR("Camera {} error 0x{:08X}: {}", camera_name, error_code, msg);
    QMetaObject::invokeMethod(this, [this, cam_name = camera_name, code = error_code]()
    {
        setStatus(QStringLiteral("错误"));
        emit cameraError(cam_name, code);
    }, Qt::QueuedConnection);
}

void CameraViewWidget::updateFrame(const HalconCpp::HObject &image)
{
    current_image_ = image;
    displayImage(current_image_);
}

void CameraViewWidget::setStatus(const QString &status)
{
    ui->label_status->setText(status);
}

void CameraViewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (hwindow_)
    {
        hwindow_.reset();
        initHalconWindow();

        if (current_image_.IsInitialized())
            displayImage(current_image_);
    }
}

