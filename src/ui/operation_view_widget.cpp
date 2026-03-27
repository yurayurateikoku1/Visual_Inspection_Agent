#include "operation_view_widget.h"
#include "ui_operation_view_widget.h"
OperationViewWidget::OperationViewWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::OperationViewWidget)
{
    ui->setupUi(this);
}

OperationViewWidget::~OperationViewWidget()
{
    delete ui;
}
