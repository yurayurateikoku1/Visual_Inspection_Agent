#include "camera_view_widget.h"
#include "ui_camera_view_widget.h"
#include <QResizeEvent>
#include <spdlog/spdlog.h>

CameraViewWidget::CameraViewWidget(const std::string &camera_id, QWidget *parent)
    : QWidget(parent), ui(new Ui::CameraViewWidget), camera_id_(camera_id)
{
    ui->setupUi(this);

    ui->label_cameraId->setText(QString::fromStdString(camera_id));
    ui->label_status->setText(QStringLiteral("离线"));
    ui->pushButton_scaleWindow->setIcon(QIcon(":/assets/fangdachuangkou2x.png"));

    // Halcon 渲染画布
    ui->widget_window->setMinimumSize(320, 240);
    ui->widget_window->setStyleSheet("background-color: #1e1e1e;");
    ui->widget_window->setAttribute(Qt::WA_NativeWindow);

    connect(ui->pushButton_scaleWindow, &QPushButton::clicked,
            this, &CameraViewWidget::onScaleWindowClicked);
}

CameraViewWidget::~CameraViewWidget()
{
    hwindow_.reset();
    delete ui;
}

void CameraViewWidget::initHalconWindow()
{
    if (hwindow_)
        return;

    int w = ui->widget_window->width();
    int h = ui->widget_window->height();
    if (w <= 0 || h <= 0)
        return;

    try
    {
        hwindow_ = std::make_unique<HalconCpp::HWindow>(
            0, 0, w - 1, h - 1,
            static_cast<Hlong>(ui->widget_window->winId()),
            "visible", "");
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("HWindow create failed: {}", e.ErrorMessage().Text());
    }
}

void CameraViewWidget::displayImage(const HalconCpp::HObject &image)
{
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

void CameraViewWidget::onScaleWindowClicked()
{
    maximized_ = !maximized_;
    ui->pushButton_scaleWindow->setIcon(maximized_ ? QIcon(":/assets/fangdachuangkou2x.png") : QIcon(":/assets/suoxiaochuangkou2x.png"));
    emit maximizeRequested(camera_id_);
}
