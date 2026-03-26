#pragma once
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CameraParamDialog;
}
QT_END_NAMESPACE

class CameraParamDialog : public QDialog
{
    Q_OBJECT
public:
    CameraParamDialog(QWidget *parent = nullptr);
    ~CameraParamDialog();
private slots:

    void on_pushButton_getCameraParam_clicked();
    void on_pushButton_setCameraParam_clicked();

private:
    Ui::CameraParamDialog *ui;
};