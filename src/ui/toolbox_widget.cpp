#include "toolbox_widget.h"
#include "ui_toolbox_widget.h"
#include "../algorithm/algorithm_factory.h"
#include "../algorithm/algorithm_interface.h"

ToolboxWidget::ToolboxWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::ToolboxWidget)
{
    ui->setupUi(this);
    ui->treeWidget_toolbox->hide(); // 默认折叠
    initToolbox();
}

ToolboxWidget::~ToolboxWidget()
{
    delete ui;
}

void ToolboxWidget::on_pushButton_toolbox_clicked()
{
    collapsed_ = !collapsed_;
    ui->treeWidget_toolbox->setVisible(!collapsed_);
}

void ToolboxWidget::on_treeWidget_toolbox_itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item || !item->parent())
        return;
    std::string algo_id = item->data(0, Qt::UserRole).toString().toStdString();
    emit algorithmActivated(algo_id);
}

void ToolboxWidget::initToolbox()
{
    ui->treeWidget_toolbox->clear();
    algo_name_cache_.clear();

    auto by_cat = AlgorithmFactory::instance().registeredByCategory();

    if (by_cat.empty())
    {
        const char *default_cats[] = {
            AlgorithmCategory::DETECTION,
            AlgorithmCategory::MEASUREMENT,
            AlgorithmCategory::IMAGE_PROCESSING,
            AlgorithmCategory::AI_INFER,
        };
        for (auto cat : default_cats)
        {
            auto *cat_item = new QTreeWidgetItem(ui->treeWidget_toolbox);
            cat_item->setText(0, QString::fromUtf8(cat));
            cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);
        }
        return;
    }

    for (auto &[category, ids] : by_cat)
    {
        auto *cat_item = new QTreeWidgetItem(ui->treeWidget_toolbox);
        cat_item->setText(0, QString::fromUtf8(category));
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);

        for (auto &id : ids)
        {
            auto algo = AlgorithmFactory::instance().create(id);
            std::string display = algo ? algo->name() : id;
            algo_name_cache_[id] = display;

            auto *algo_item = new QTreeWidgetItem(cat_item);
            algo_item->setText(0, QString::fromStdString(display));
            algo_item->setData(0, Qt::UserRole, QString::fromStdString(id));
        }
    }

    ui->treeWidget_toolbox->expandAll();
}

QString ToolboxWidget::algoDisplayName(const std::string &algo_id) const
{
    auto it = algo_name_cache_.find(algo_id);
    if (it != algo_name_cache_.end())
        return QString::fromStdString(it->second);
    return QString::fromStdString(algo_id);
}
