#include "camera_view_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <opencv2/imgproc.hpp>

CameraViewWidget::CameraViewWidget(const std::string &camera_id, QWidget *parent)
    : QWidget(parent), camera_id_(camera_id)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    // 顶部栏：状态标签 + 放大按钮
    auto *top_bar = new QHBoxLayout;
    status_label_ = new QLabel(QString::fromStdString(camera_id));
    status_label_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    status_label_->setFixedHeight(24);

    maximize_btn_ = new QPushButton(u8"\u2922"); // ⤢ 放大符号
    maximize_btn_->setFixedSize(24, 24);
    maximize_btn_->setToolTip(u8"放大/还原");

    top_bar->addWidget(status_label_, 1);
    top_bar->addWidget(maximize_btn_);
    layout->addLayout(top_bar);

    // 图像显示
    image_label_ = new QLabel;
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setMinimumSize(320, 240);
    image_label_->setStyleSheet("background-color: #1e1e1e;");
    layout->addWidget(image_label_, 1);

    connect(this, &CameraViewWidget::frameUpdated,
            this, &CameraViewWidget::onFrameUpdated, Qt::QueuedConnection);
    connect(maximize_btn_, &QPushButton::clicked,
            this, &CameraViewWidget::onMaximizeBtnClicked);
}

void CameraViewWidget::updateFrame(const cv::Mat &frame)
{
    if (frame.empty())
        return;

    cv::Mat rgb;
    if (frame.channels() == 1)
    {
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    }
    else
    {
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    }

    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                QImage::Format_RGB888);
    emit frameUpdated(qimg.copy());
}

void CameraViewWidget::setStatus(const QString &status)
{
    status_label_->setText(status);
}

void CameraViewWidget::onFrameUpdated(const QImage &image)
{
    image_label_->setPixmap(QPixmap::fromImage(image).scaled(
        image_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraViewWidget::onMaximizeBtnClicked()
{
    maximized_ = !maximized_;
    maximize_btn_->setText(maximized_ ? u8"\u2923" : u8"\u2922"); // ⤣ 还原 / ⤢ 放大
    emit maximizeRequested(camera_id_);
}
