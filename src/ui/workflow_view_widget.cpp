#include "workflow_view_widget.h"
#include "ui_workflow_view_widget.h"
#include "toolbox_widget.h"
#include "camera_view_widget.h"
#include "../app/app_context.h"
#include "../algorithm/algorithm_interface.h"
#include <spdlog/spdlog.h>
#include <QMessageBox>

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
    auto &wf_map = AppContext::getInstance().workflowParams();
    auto wf_it = wf_map.find(camera_name);
    if (wf_it != wf_map.end())
    {
        current_algo_ids_ = wf_it->second.algorithm_ids;
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

void WorkflowViewWidget::on_pushButton_config_clicked()
{
    int row = ui->listWidget_pipeline->currentRow();
    if (row < 0 || row >= static_cast<int>(current_algo_ids_.size()))
    {
        QMessageBox::information(this, "配置", "请先选中一个算法步骤");
        return;
    }

    // 创建临时算法实例用于配置
    auto algo = createAlgorithm(current_algo_ids_[row]);
    if (!algo)
    {
        QMessageBox::warning(this, "配置", "无法创建算法实例");
        return;
    }

    // 获取对应相机的 CameraViewWidget
    CameraViewWidget *view = nullptr;
    if (camera_view_finder_)
        view = camera_view_finder_(current_camera_name_);

    // 找到对应的 WorkflowParam，确保 algorithm_params 大小与 algorithm_ids 一致
    auto &wf_map = AppContext::getInstance().workflowParams();
    auto wf_it = wf_map.find(current_camera_name_);
    if (wf_it != wf_map.end())
    {
        auto &wp = wf_it->second;
        wp.algorithm_params.resize(wp.algorithm_ids.size());

        // 先加载已有参数，再调用 configure
        if (!wp.algorithm_params[row].is_null())
            algo->loadParams(wp.algorithm_params[row]);

        algo->configure(view, this, wp.algorithm_params[row]);
    }

    // 配置变更后重建 Pipeline
    emit workflowChanged(current_camera_name_, current_algo_ids_);
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
    auto &wf_map = AppContext::getInstance().workflowParams();
    auto wf_it = wf_map.find(current_camera_name_);
    if (wf_it != wf_map.end())
    {
        wf_it->second.algorithm_ids = current_algo_ids_;
    }

    emit workflowChanged(current_camera_name_, current_algo_ids_);
}
