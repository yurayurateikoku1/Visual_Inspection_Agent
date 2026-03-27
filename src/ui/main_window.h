#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <QLabel>
#include <QKeyEvent>
#include <map>
#include <string>
#include <vector>
#include "../camera/camera_interface.h"

class CameraViewWidget;
class IOParamDialog;
class CameraParamDialog;
class ToolboxWidget;
class WorkflowViewWidget;
class OperationViewWidget;

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
    void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) override;
    void cameraErrorReceived(const std::string &camera_name, int error_code, const std::string &msg) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:

    void on_action_parameter_triggered();
    void on_action_camera_triggered();
    void on_action_open_folder_triggered();
    void slot_cameraMaximizeRequested(const std::string &camera_name);
    void slot_cameraSelected(const std::string &camera_name);

private:
    /// @brief 初始化相机
    void initCameras();
    /// @brief 动态添加新发现的相机 UI（view + 状态灯）
    void addCameraUI(const std::string &cam_name);
    /// @brief 按 2x2 田字格布局相机 widget
    void layoutCameraViews();

    /// @brief 初始化 Modbus 通信
    void initModbusCommunication();

    /// @brief 初始化工作流
    void initWorkflow();

    /// @brief 初始化状态栏
    void initStatusBar();
    /// @brief 设置相机状态灯 (true=绿灯在线, false=灰灯离线)
    void setCameraLed(const std::string &camera_name, bool online);
    /// @brief 设置通信状态灯
    void setCommLed(const std::string &comm_name, bool connected);
    /// @brief 设置光源状态灯
    void setLightLed(bool connected);

    Ui::MainWindow *ui;
    QGridLayout *camera_layout_ = nullptr;                   // 相机区域网格布局
    std::map<std::string, CameraViewWidget *> camera_views_; // camera_name → 相机控件
    std::vector<CameraViewWidget *> camera_view_list_;       // 保持插入顺序
    std::string maximized_camera_name_;                      // 当前放大的相机
    std::map<std::string, QLabel *> camera_status_leds_;     // camera_name → 状态灯
    std::map<std::string, QLabel *> comm_status_leds_;       // comm_name → 状态灯
    QLabel *light_status_led_ = nullptr;                     // 光源状态灯

    IOParamDialog *ioparam_dialog_ = nullptr;
    CameraParamDialog *camera_param_dialog_ = nullptr;
    ToolboxWidget *toolbox_widget_ = nullptr;
    WorkflowViewWidget *workflow_view_widget_ = nullptr;
    OperationViewWidget *operation_view_widget_ = nullptr;

    std::string selected_camera_name_;

    // 离线测试
    std::vector<QString> offline_image_paths_; // 离线图片路径列表
    int offline_image_index_ = 0;              // 当前图片索引

    /// @brief 用离线图片执行一次检测
    void runOfflineTest(const QString &image_path);
};
