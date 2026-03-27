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
    void slot_cameraMaximizeRequested(const std::string &camera_name);
    void slot_cameraSelected(const std::string &camera_name);
    void slot_onCameraError(const std::string &camera_name, int error_code);
    void slot_onWorkflowSelected(const std::string &camera_name, const std::string &workflow_name);

private:
    void initCameras();
    void addCameraUI(const std::string &cam_name);
    void layoutCameraViews();
    void initModbusCommunication();
    void initWorkflow();
    void initStatusBar();
    void setCameraLed(const std::string &camera_name, bool online);
    void setCommLed(const std::string &comm_name, bool connected);
    void setLightLed(bool connected);
    void runOfflineTest(const QString &image_path);

    Ui::MainWindow *ui;
    QGridLayout *camera_layout_ = nullptr;
    std::map<std::string, CameraViewWidget *> camera_views_;
    std::vector<CameraViewWidget *> camera_view_list_;
    std::string maximized_camera_name_;
    std::map<std::string, QLabel *> camera_status_leds_;
    std::map<std::string, QLabel *> comm_status_leds_;
    QLabel *light_status_led_ = nullptr;

    IOParamDialog *ioparam_dialog_               = nullptr;
    CameraParamDialog *camera_param_dialog_      = nullptr;
    WorkflowViewWidget *workflow_view_widget_    = nullptr;
    OperationViewWidget *operation_view_widget_  = nullptr;

    std::string selected_camera_name_;

    /// camera_name → 当前选中的 workflow_name（离线测试用）
    std::map<std::string, std::string> selected_workflow_;

    std::vector<QString> offline_image_paths_;
    int offline_image_index_ = 0;
};
