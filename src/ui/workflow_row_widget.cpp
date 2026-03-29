#include "workflow_row_widget.h"
#include "ui_workflow_row_widget.h"

WorkflowRowWidget::WorkflowRowWidget(const WorkflowParam &wp, QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowRowWidget), workflow_key_(wp.key())
{
    ui->setupUi(this);

    ui->label_di->setText(QString("DI%1").arg(wp.trigger.di_addr));
    ui->label_name->setText(QString::fromStdString(workflow_key_));
    ui->checkBox_enabled->setChecked(wp.enabled);

    connect(ui->checkBox_enabled, &QCheckBox::toggled, this,
            [this](bool checked) { emit enabledChanged(workflow_key_, checked); });

    connect(ui->pushButton_config, &QPushButton::clicked, this,
            [this]() { emit configClicked(workflow_key_); });
}

WorkflowRowWidget::~WorkflowRowWidget()
{
    delete ui;
}
