#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <opencv2/core.hpp>
#include <string>

/// @brief 相机view
class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraViewWidget(const std::string &camera_id, QWidget *parent = nullptr);

    void updateFrame(const cv::Mat &frame);
    void setStatus(const QString &status);
    std::string cameraId() const { return camera_id_; }

signals:
    void frameUpdated(const QImage &image);
    void maximizeRequested(const std::string &camera_id);

private slots:
    void onFrameUpdated(const QImage &image);
    void onMaximizeBtnClicked();

private:
    std::string camera_id_;
    QLabel *image_label_;
    QLabel *status_label_;
    QPushButton *maximize_btn_;
    bool maximized_ = false;
};
