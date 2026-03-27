#pragma once

#include <QWidget>
#include <QTreeWidgetItem>
#include <string>
#include <map>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ToolboxWidget;
}
QT_END_NAMESPACE

/// @brief 全局算法工具箱（可折叠），按分类显示所有已注册的算法
class ToolboxWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolboxWidget(QWidget *parent = nullptr);
    ~ToolboxWidget() override;

    /// @brief 获取算法显示名称
    QString algoDisplayName(const std::string &algo_id) const;

signals:
    /// @brief 双击算法条目时发出，携带算法 ID
    void algorithmActivated(const std::string &algo_id);

private slots:
    void on_pushButton_toolbox_clicked();
    void on_treeWidget_toolbox_itemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void initToolbox();

    Ui::ToolboxWidget *ui;
    bool collapsed_ = true;
    std::map<std::string, std::string> algo_name_cache_;
};
