#pragma once

#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <string>
#include <memory>
#include "../camera/camera_interface.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CameraViewWidget;
}
QT_END_NAMESPACE

/// @brief 相机 view（基于 Halcon HWindow 渲染，支持叠加图形/ROI 等操作）
class CameraViewWidget : public QWidget, public ICameraCallback
{
    Q_OBJECT
public:
    explicit CameraViewWidget(const std::string &camera_name, QWidget *parent = nullptr);
    ~CameraViewWidget() override;

    // ICameraCallback
    void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) override;
    void cameraErrorReceived(const std::string &camera_name, int error_code, const std::string &msg) override;

    /// @brief 显示 Halcon HObject 图像
    void updateFrame(const HalconCpp::HObject &image);

    void setStatus(const QString &status);
    std::string cameraName() const { return camera_name_; }

    /// @brief 获取 Halcon 窗口句柄（供外部叠加图形、画 ROI 等）
    HalconCpp::HWindow *halconWindow() { return hwindow_.get(); }

signals:
    /// @brief 帧到达（已在主线程，可直接连接 WorkflowManager）
    void frameArrived(const std::string &camera_name, const HalconCpp::HObject &frame);
    /// @brief 相机错误
    void cameraError(const std::string &camera_name, int error_code);
    void maximizeRequested(const std::string &camera_name);
    void selected(const std::string &camera_name);

private slots:

    void on_pushButton_scaleWindow_clicked();
    void on_pushButton_workflow_clicked();

private:
    void initHalconWindow();
    void displayImage(const HalconCpp::HObject &image);
    void resizeEvent(QResizeEvent *event) override;

    Ui::CameraViewWidget *ui;
    std::string camera_name_;
    bool maximized_ = false;


    std::unique_ptr<HalconCpp::HWindow> hwindow_; // Halcon 窗口
    int hwindow_w_ = 0;                           // HWindow 创建时的宽度
    int hwindow_h_ = 0;                           // HWindow 创建时的高度
    HalconCpp::HObject current_image_;            // 当前显示的图像（缓存，用于 resize 重绘）
};
