#pragma once

#include <QWidget>
#include <string>
#include <functional>

class QListWidgetItem;
class CameraViewWidget;
class WorkflowRowWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class WorkflowViewWidget; }
QT_END_NAMESPACE

class WorkflowViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowViewWidget(QWidget *parent = nullptr);
    ~WorkflowViewWidget() override;

    void loadWorkflow(const std::string &camera_name);
    void setSelectedWorkflow(const std::string &workflow_name);

    using CameraViewFinder = std::function<CameraViewWidget *(const std::string &)>;
    void setCameraViewFinder(CameraViewFinder finder) { camera_view_finder_ = std::move(finder); }

signals:
    void workflowSelected(const std::string &camera_name, const std::string &workflow_name);
    void workflowConfigChanged(const std::string &workflow_name);

private:
    void refreshList();
    void onConfigClicked(const std::string &workflow_name);

    Ui::WorkflowViewWidget *ui;
    std::string current_camera_name_;
    CameraViewFinder camera_view_finder_;
};
