#include "workflow_view_widget.h"
#include "ui_workflow_view_widget.h"
#include "toolbox_widget.h"
#include "../app/app_context.h"
#include <spdlog/spdlog.h>

WorkflowViewWidget::WorkflowViewWidget(ToolboxWidget *toolbox, QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowViewWidget), toolbox_(toolbox)
{
    ui->setupUi(this);
}

WorkflowViewWidget::~WorkflowViewWidget()
{
    delete ui;
}

void WorkflowViewWidget::loadWorkflow(const std::string &camera_name)
{
    current_camera_name_ = camera_name;
    ui->label_camera->setText(QString::fromStdString(camera_name));

    current_algo_ids_.clear();
    for (auto &wp : AppContext::getInstance().workflowParams())
    {
        if (wp.camera_name == camera_name)
        {
            current_algo_ids_ = wp.algorithm_ids;
            break;
        }
    }

    syncPipelineList();
}

void WorkflowViewWidget::addAlgorithm(const std::string &algo_id)
{
    if (current_camera_name_.empty())
        return;

    current_algo_ids_.push_back(algo_id);
    syncPipelineList();
    commitChanges();
}

void WorkflowViewWidget::on_pushButton_down_clicked()
{
    int row = ui->listWidget_pipeline->currentRow();
    if (row < 0 || row >= static_cast<int>(current_algo_ids_.size()) - 1)
        return;

    std::swap(current_algo_ids_[row], current_algo_ids_[row + 1]);
    syncPipelineList();
    commitChanges();
    ui->listWidget_pipeline->setCurrentRow(row + 1);
}

void WorkflowViewWidget::on_pushButton_up_clicked()
{
    int row = ui->listWidget_pipeline->currentRow();
    if (row <= 0)
        return;

    std::swap(current_algo_ids_[row], current_algo_ids_[row - 1]);
    syncPipelineList();
    commitChanges();
    ui->listWidget_pipeline->setCurrentRow(row - 1);
}

void WorkflowViewWidget::on_pushButton_remove_clicked()
{
    int row = ui->listWidget_pipeline->currentRow();
    if (row < 0 || row >= static_cast<int>(current_algo_ids_.size()))
        return;

    current_algo_ids_.erase(current_algo_ids_.begin() + row);
    syncPipelineList();
    commitChanges();

    if (!current_algo_ids_.empty())
        ui->listWidget_pipeline->setCurrentRow(std::min(row, static_cast<int>(current_algo_ids_.size()) - 1));
}

void WorkflowViewWidget::syncPipelineList()
{
    ui->listWidget_pipeline->clear();
    for (size_t i = 0; i < current_algo_ids_.size(); ++i)
    {
        QString name = toolbox_ ? toolbox_->algoDisplayName(current_algo_ids_[i])
                                : QString::fromStdString(current_algo_ids_[i]);
        QString text = QString("%1. %2").arg(i + 1).arg(name);
        ui->listWidget_pipeline->addItem(text);
    }
}

void WorkflowViewWidget::commitChanges()
{
    for (auto &wp : AppContext::getInstance().workflowParams())
    {
        if (wp.camera_name == current_camera_name_)
        {
            wp.algorithm_ids = current_algo_ids_;
            break;
        }
    }

    emit workflowChanged(current_camera_name_, current_algo_ids_);
}
