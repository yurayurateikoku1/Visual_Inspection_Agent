#pragma once
#include <QWidget>
#include "../app/common.h"

QT_BEGIN_NAMESPACE
namespace Ui { class WorkflowRowWidget; }
QT_END_NAMESPACE

class WorkflowRowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowRowWidget(const WorkflowParam &wp, QWidget *parent = nullptr);
    ~WorkflowRowWidget() override;

    const std::string &workflowKey() const { return workflow_key_; }

signals:
    void configClicked(const std::string &workflow_name);
    void enabledChanged(const std::string &workflow_name, bool enabled);

private:
    Ui::WorkflowRowWidget *ui;
    std::string workflow_key_;
};
