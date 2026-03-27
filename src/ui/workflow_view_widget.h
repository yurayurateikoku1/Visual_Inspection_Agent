#pragma once

#include <QWidget>
#include <string>
#include <vector>

class ToolboxWidget;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class WorkflowViewWidget;
}
QT_END_NAMESPACE

/// @brief 单个相机的工作流编辑面板（流程列表 + 操作按钮）
class WorkflowViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowViewWidget(ToolboxWidget *toolbox, QWidget *parent = nullptr);
    ~WorkflowViewWidget() override;

    /// @brief 加载指定相机的工作流到界面
    void loadWorkflow(const std::string &camera_name);

    /// @brief 添加算法到当前流程
    void addAlgorithm(const std::string &algo_id);

signals:
    /// @brief 用户修改了流程列表（添加/删除/排序）
    void workflowChanged(const std::string &camera_name,
                         const std::vector<std::string> &algorithm_ids);

private slots:

    void on_pushButton_down_clicked();
    void on_pushButton_up_clicked();
    void on_pushButton_remove_clicked();

private:
    void syncPipelineList();
    void commitChanges();

    Ui::WorkflowViewWidget *ui;
    ToolboxWidget *toolbox_; // 引用全局工具箱（用于获取显示名）
    std::string current_camera_name_;
    std::vector<std::string> current_algo_ids_;
};
