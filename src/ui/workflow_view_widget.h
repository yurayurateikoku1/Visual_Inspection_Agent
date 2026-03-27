#pragma once

#include <QWidget>
#include <string>
#include <functional>

class QTableWidget;
class CameraViewWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class WorkflowViewWidget; }
QT_END_NAMESPACE

/// @brief 工作流配置面板
///        固定显示当前相机的4条 WorkflowParam（对应 DI0~DI3）
///        点击行选中（用于离线测试），点击[配置]弹出 WorkflowConfigDialog
class WorkflowViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowViewWidget(QWidget *parent = nullptr);
    ~WorkflowViewWidget() override;

    /// @brief 切换到指定相机的工作流列表
    void loadWorkflow(const std::string &camera_name);

    /// @brief 恢复高亮选中行（切换相机后由 MainWindow 调用）
    void setSelectedWorkflow(const std::string &workflow_name);

    using CameraViewFinder = std::function<CameraViewWidget *(const std::string &)>;
    void setCameraViewFinder(CameraViewFinder finder) { camera_view_finder_ = std::move(finder); }

signals:
    /// @brief 用户点击某行，选中该 workflow 作为当前离线测试目标
    void workflowSelected(const std::string &camera_name, const std::string &workflow_name);

    /// @brief 用户在 Dialog 中修改了参数，需要重建对应 Pipeline
    void workflowConfigChanged(const std::string &workflow_name);

private slots:
    void onTableRowClicked(int row, int col);
    void onConfigClicked(int row);

private:
    void refreshTable();

    Ui::WorkflowViewWidget *ui;
    std::string current_camera_name_;
    CameraViewFinder camera_view_finder_;
};
