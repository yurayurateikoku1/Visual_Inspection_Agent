#include "workflow_view_widget.h"
#include "ui_workflow_view_widget.h"
#include "workflow_config_dialog.h"
#include "camera_view_widget.h"
#include "../app/app_context.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <spdlog/spdlog.h>

WorkflowViewWidget::WorkflowViewWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowViewWidget)
{
    ui->setupUi(this);

    // 固定4行，列宽自适应
    ui->tableWidget_workflows->setRowCount(4);
    ui->tableWidget_workflows->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget_workflows->verticalHeader()->setVisible(false);

    connect(ui->tableWidget_workflows, &QTableWidget::cellClicked,
            this, &WorkflowViewWidget::onTableRowClicked);
}

WorkflowViewWidget::~WorkflowViewWidget()
{
    delete ui;
}

void WorkflowViewWidget::loadWorkflow(const std::string &camera_name)
{
    current_camera_name_ = camera_name;
    ui->label_camera->setText(QString::fromStdString(camera_name));
    refreshTable();
}

void WorkflowViewWidget::setSelectedWorkflow(const std::string &workflow_name)
{
    if (workflow_name.empty())
        return;

    auto wf_names = AppContext::getInstance().workflowNamesForCamera(current_camera_name_);
    for (int row = 0; row < static_cast<int>(wf_names.size()); ++row)
    {
        if (wf_names[row] == workflow_name)
        {
            ui->tableWidget_workflows->selectRow(row);
            return;
        }
    }
}

void WorkflowViewWidget::refreshTable()
{
    auto &ctx      = AppContext::getInstance();
    auto wf_names  = ctx.workflowNamesForCamera(current_camera_name_);

    for (int row = 0; row < 4; ++row)
    {
        auto *item_di = new QTableWidgetItem(QString("DI%1").arg(row));
        item_di->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_workflows->setItem(row, 0, item_di);

        if (row < static_cast<int>(wf_names.size()))
        {
            const auto &wf_name = wf_names[row];
            auto it = ctx.workflowParams().find(wf_name);
            if (it == ctx.workflowParams().end())
                continue;
            const auto &wp = it->second;

            ui->tableWidget_workflows->setItem(row, 1,
                new QTableWidgetItem(QString::fromStdString(wp.name)));

            auto *check  = new QCheckBox();
            check->setChecked(wp.enabled);
            check->setEnabled(false);
            auto *cell_widget = new QWidget();
            auto *cell_layout = new QHBoxLayout(cell_widget);
            cell_layout->addWidget(check);
            cell_layout->setAlignment(Qt::AlignCenter);
            cell_layout->setContentsMargins(0, 0, 0, 0);
            ui->tableWidget_workflows->setCellWidget(row, 2, cell_widget);

            auto *btn_config = new QPushButton(QStringLiteral("配置"));
            connect(btn_config, &QPushButton::clicked, this, [this, row]()
                    { onConfigClicked(row); });
            ui->tableWidget_workflows->setCellWidget(row, 3, btn_config);
        }
        else
        {
            ui->tableWidget_workflows->setItem(row, 1, new QTableWidgetItem(QStringLiteral("-")));
            ui->tableWidget_workflows->setCellWidget(row, 2, nullptr);
            ui->tableWidget_workflows->setCellWidget(row, 3, nullptr);
        }
    }
}

void WorkflowViewWidget::onTableRowClicked(int row, int /*col*/)
{
    if (current_camera_name_.empty())
        return;

    auto wf_names = AppContext::getInstance().workflowNamesForCamera(current_camera_name_);
    if (row >= static_cast<int>(wf_names.size()))
        return;

    emit workflowSelected(current_camera_name_, wf_names[row]);
}

void WorkflowViewWidget::onConfigClicked(int row)
{
    auto &ctx     = AppContext::getInstance();
    auto wf_names = ctx.workflowNamesForCamera(current_camera_name_);
    if (row >= static_cast<int>(wf_names.size()))
        return;

    const auto &wf_name = wf_names[row];
    auto it = ctx.workflowParams().find(wf_name);
    if (it == ctx.workflowParams().end())
        return;

    CameraViewWidget *view = nullptr;
    if (camera_view_finder_)
        view = camera_view_finder_(current_camera_name_);

    WorkflowConfigDialog dlg(it->second, view, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshTable();
        emit workflowConfigChanged(wf_name);
    }
}
