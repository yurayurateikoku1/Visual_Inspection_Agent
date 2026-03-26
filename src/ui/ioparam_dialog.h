#pragma once
#include <QDialog>
#include <QCheckBox>
#include <array>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class IOParamDialog;
}
QT_END_NAMESPACE

class IOParamDialog : public QDialog
{
    Q_OBJECT
public:
    IOParamDialog(QWidget *parent = nullptr);
    ~IOParamDialog();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    /// @brief IO 状态更新 → 刷新 CheckBox
    void slot_updateIODisplay();
    /// @brief 保存参数按钮
    void on_pushButton_setParam_clicked();
    /// @brief DO 输出 CheckBox 点击 → 写线圈到 PLC
    void slot_doCheckBoxClicked(int index, bool checked);

private:
    /// @brief 从 ConfigManager 加载参数到 UI
    void loadParam();
    /// @brief 将 UI 参数保存到 ConfigManager
    void saveParam();
    /// @brief 将光源亮度下发（根据 use_modbus 选择串口或 Modbus）
    void writeLuminance();

    Ui::IOParamDialog *ui;
    std::array<QCheckBox *, 8> di_checkboxes_{};
    std::array<QCheckBox *, 8> do_checkboxes_{};
};
