#pragma once

#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <string>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CameraViewWidget;
}
QT_END_NAMESPACE

/// @brief 相机 view（基于 Halcon HWindow 渲染，支持叠加图形/ROI 等操作）
class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraViewWidget(const std::string &camera_id, QWidget *parent = nullptr);
    ~CameraViewWidget() override;

    /// @brief 显示 Halcon HObject 图像
    void updateFrame(const HalconCpp::HObject &image);

    void setStatus(const QString &status);
    std::string cameraId() const { return camera_id_; }

    /// @brief 获取 Halcon 窗口句柄（供外部叠加图形、画 ROI 等）
    HalconCpp::HWindow *halconWindow() { return hwindow_.get(); }

signals:
    void maximizeRequested(const std::string &camera_id);

private slots:
    void onScaleWindowClicked();

private:
    void initHalconWindow();
    void displayImage(const HalconCpp::HObject &image);
    void resizeEvent(QResizeEvent *event) override;

    Ui::CameraViewWidget *ui;
    std::string camera_id_;
    bool maximized_ = false;

    std::unique_ptr<HalconCpp::HWindow> hwindow_; // Halcon 窗口
    HalconCpp::HObject current_image_;            // 当前显示的图像（缓存，用于 resize 重绘）
};
