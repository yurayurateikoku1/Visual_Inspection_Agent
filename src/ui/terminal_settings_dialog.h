#pragma once
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class TerminalSettingsDialog;
}
QT_END_NAMESPACE

class TerminalSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TerminalSettingsDialog(QWidget *parent = nullptr);
    ~TerminalSettingsDialog();

private:
    Ui::TerminalSettingsDialog *ui;
};