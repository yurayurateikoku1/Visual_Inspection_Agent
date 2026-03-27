#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class OperationViewWidget;
}
QT_END_NAMESPACE

class OperationViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OperationViewWidget(QWidget *parent = nullptr);
    ~OperationViewWidget() override;

private:
    Ui::OperationViewWidget *ui;
};