#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <map>
#include <string>
#include "../camera/camera_interface.h"

class CameraViewWidget;
class ParamPage;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public ICameraCallback
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // ICameraCallback
    void onFrameReceived(const std::string &camera_id, const cv::Mat &frame) override;
    void onCameraError(const std::string &camera_id, int error_code, const std::string &msg) override;

private slots:
    void on_action_parameter_triggered();
    void onCameraMaximizeRequested(const std::string &camera_id);

private:
    /// @brief 初始化相机
    void initCameras();
    /// @brief 按 2x2 田字格布局相机 widget
    void layoutCameraViews();

    Ui::MainWindow *ui;
    QGridLayout *camera_layout_ = nullptr;
    std::map<std::string, CameraViewWidget *> camera_views_;
    std::vector<CameraViewWidget *> camera_view_list_; // 保持插入顺序
    std::string maximized_camera_id_;
    ParamPage *param_page_ = nullptr;
};
