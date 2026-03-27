#include "toolbox_widget.h"
#include "ui_toolbox_widget.h"

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

    // 静态工具列表
    struct ToolEntry
    {
        const char *id;
        const char *display;
    };
    struct CategoryEntry
    {
        const char *category;
        std::vector<ToolEntry> tools;
    };

    std::vector<CategoryEntry> categories = {
        {"工具", {{"DrawROI", "绘制ROI"}}},
        {"AI推理", {{"YoloTerminal", "YOLO端子检测"}}},
    };

    for (auto &cat : categories)
    {
        auto *cat_item = new QTreeWidgetItem(ui->treeWidget_toolbox);
        cat_item->setText(0, QString::fromUtf8(cat.category));
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);

        for (auto &tool : cat.tools)
        {
            algo_name_cache_[tool.id] = tool.display;

            auto *algo_item = new QTreeWidgetItem(cat_item);
            algo_item->setText(0, QString::fromUtf8(tool.display));
            algo_item->setData(0, Qt::UserRole, QString::fromStdString(tool.id));
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
