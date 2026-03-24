#pragma once
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ParamPage;
}
QT_END_NAMESPACE

class ParamPage : public QDialog
{
    Q_OBJECT
public:
    ParamPage(QWidget *parent = nullptr);
    ~ParamPage();

private:
    Ui::ParamPage *ui;
};
