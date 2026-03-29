#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <QLabel>
#include <QKeyEvent>
#include <map>
#include <string>
#include <vector>

class CameraViewWidget;
class IOParamDialog;
class CameraParamDialog;
class WorkflowViewWidget;
class OperationViewWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_action_parameter_triggered();
    void on_action_camera_triggered();
    void on_action_open_folder_triggered();

private:
    void initCameras();
    void addCameraUI(const std::string &cam_name);
    void layoutCameraViews();
    void initModbusCommunication();
    void initWorkflow();
    void initStatusBar();
    void setCommLed(const std::string &comm_name, bool connected);
    void setLightLed(bool connected);
    void runOfflineTest(const QString &image_path);

    Ui::MainWindow *ui;
    QGridLayout *camera_layout_ = nullptr;          ///< 相机画面区域的网格布局（最多2×2）
    std::vector<CameraViewWidget *> camera_view_list_; ///< 所有相机视图，按添加顺序排列，用于布局和按名查找
    std::string maximized_camera_name_;             ///< 当前全屏相机名，空字符串表示网格模式
    std::map<std::string, QLabel *> comm_status_leds_;   ///< 状态栏通信通道连接指示灯（comm_name → led）
    QLabel *light_status_led_ = nullptr;            ///< 状态栏光源控制器连接指示灯

    IOParamDialog *ioparam_dialog_               = nullptr;
    CameraParamDialog *camera_param_dialog_      = nullptr;
    WorkflowViewWidget *workflow_view_widget_    = nullptr;
    OperationViewWidget *operation_view_widget_  = nullptr;

    std::vector<QString> offline_image_paths_;
    int offline_image_index_ = 0;
};
