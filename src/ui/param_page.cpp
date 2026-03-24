#include "param_page.h"
#include "ui_param_page.h"

ParamPage::ParamPage(QWidget *parent)
    : QDialog(parent), ui(new Ui::ParamPage)
{
    ui->setupUi(this);
}

ParamPage::~ParamPage()
{
    delete ui;
}
