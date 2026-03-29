#include "workflow_view_widget.h"
#include "ui_workflow_view_widget.h"
#include "workflow_row_widget.h"
#include "workflow_config_dialog.h"
#include "camera_view_widget.h"
#include "../app/app_context.h"
#include <QListWidgetItem>

WorkflowViewWidget::WorkflowViewWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowViewWidget)
{
    ui->setupUi(this);

    connect(ui->listWidget_workflows, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item)
            {
                if (current_camera_name_.empty()) return;
                auto *row = qobject_cast<WorkflowRowWidget *>(
                    ui->listWidget_workflows->itemWidget(item));
                if (row)
                    emit workflowSelected(current_camera_name_, row->workflowKey());
            });
}

WorkflowViewWidget::~WorkflowViewWidget()
{
    delete ui;
}

void WorkflowViewWidget::loadWorkflow(const std::string &camera_name)
{
    current_camera_name_ = camera_name;
    ui->label_camera->setText(QString::fromStdString(camera_name));
    refreshList();
}

void WorkflowViewWidget::setSelectedWorkflow(const std::string &workflow_name)
{
    for (int i = 0; i < ui->listWidget_workflows->count(); ++i)
    {
        auto *item = ui->listWidget_workflows->item(i);
        auto *row  = qobject_cast<WorkflowRowWidget *>(
            ui->listWidget_workflows->itemWidget(item));
        if (row && row->workflowKey() == workflow_name)
        {
            ui->listWidget_workflows->setCurrentItem(item);
            return;
        }
    }
}

void WorkflowViewWidget::refreshList()
{
    ui->listWidget_workflows->clear();

    for (const auto &wf_name : AppContext::getInstance().workflowKeysForCamera(current_camera_name_))
    {
        auto &params = AppContext::getInstance().workflow_params;
        auto it = params.find(wf_name);
        if (it == params.end()) continue;

        auto *row  = new WorkflowRowWidget(it->second);
        auto *item = new QListWidgetItem(ui->listWidget_workflows);
        item->setSizeHint(row->sizeHint());
        ui->listWidget_workflows->setItemWidget(item, row);

        connect(row, &WorkflowRowWidget::enabledChanged, this,
                [](const std::string &name, bool enabled)
                {
                    auto &p = AppContext::getInstance().workflow_params;
                    if (auto it2 = p.find(name); it2 != p.end())
                        it2->second.enabled = enabled;
                });

        connect(row, &WorkflowRowWidget::configClicked, this,
                &WorkflowViewWidget::onConfigClicked);
    }
}

void WorkflowViewWidget::onConfigClicked(const std::string &workflow_name)
{
    auto &params = AppContext::getInstance().workflow_params;
    auto it = params.find(workflow_name);
    if (it == params.end()) return;

    CameraViewWidget *view = camera_view_finder_ ? camera_view_finder_(current_camera_name_) : nullptr;

    WorkflowConfigDialog dlg(it->second, view, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshList();
        emit workflowConfigChanged(workflow_name);
    }
}
