#include "workflow_config_dialog.h"
#include "camera_view_widget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <spdlog/spdlog.h>

WorkflowConfigDialog::WorkflowConfigDialog(WorkflowParam &param,
                                             CameraViewWidget *view,
                                             QWidget *parent)
    : QDialog(parent), param_(param), view_(view)
{
    setWindowTitle(QStringLiteral("工作流配置 - %1").arg(QString::fromStdString(param_.name)));
    setMinimumWidth(420);
    buildUi();
    loadToUi();
}

void WorkflowConfigDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ── 基本参数 ──────────────────────────────────────────
    auto *grp_basic  = new QGroupBox(QStringLiteral("基本参数"), this);
    auto *form_basic = new QFormLayout(grp_basic);

    edit_name_     = new QLineEdit(this);
    check_enabled_ = new QCheckBox(QStringLiteral("启用"), this);
    spin_di_addr_  = new QSpinBox(this);   spin_di_addr_->setRange(0, 3);
    spin_delay_ms_ = new QSpinBox(this);   spin_delay_ms_->setRange(0, 5000); spin_delay_ms_->setSuffix(" ms");
    spin_do_ok_    = new QSpinBox(this);   spin_do_ok_->setRange(0, 999);
    spin_do_ng_    = new QSpinBox(this);   spin_do_ng_->setRange(0, 999);
    spin_exposure_ = new QDoubleSpinBox(this);
    spin_exposure_->setRange(-1.0, 200000.0);
    spin_exposure_->setDecimals(0);
    spin_exposure_->setSuffix(" us");
    spin_exposure_->setSpecialValueText(QStringLiteral("不覆盖"));

    form_basic->addRow(QStringLiteral("名称"),     edit_name_);
    form_basic->addRow(QString(),                  check_enabled_);
    form_basic->addRow(QStringLiteral("DI 触发"),  spin_di_addr_);
    form_basic->addRow(QStringLiteral("触发延时"), spin_delay_ms_);
    form_basic->addRow(QStringLiteral("DO OK"),    spin_do_ok_);
    form_basic->addRow(QStringLiteral("DO NG"),    spin_do_ng_);
    form_basic->addRow(QStringLiteral("曝光覆盖"), spin_exposure_);
    root->addWidget(grp_basic);

    // ── ROI 裁剪 ──────────────────────────────────────────
    group_roi_  = new QGroupBox(QStringLiteral("ROI 裁剪"), this);
    group_roi_->setCheckable(false);
    auto *form_roi = new QFormLayout(group_roi_);

    check_roi_ = new QCheckBox(QStringLiteral("启用 ROI 裁剪"), this);
    spin_row1_ = new QDoubleSpinBox(this); spin_row1_->setRange(0, 99999);
    spin_col1_ = new QDoubleSpinBox(this); spin_col1_->setRange(0, 99999);
    spin_row2_ = new QDoubleSpinBox(this); spin_row2_->setRange(0, 99999);
    spin_col2_ = new QDoubleSpinBox(this); spin_col2_->setRange(0, 99999);

    auto *btn_draw = new QPushButton(QStringLiteral("在相机画面上画框"), this);
    connect(btn_draw, &QPushButton::clicked, this, &WorkflowConfigDialog::onDrawRoi);

    form_roi->addRow(QString(), check_roi_);
    form_roi->addRow(QStringLiteral("Row1"), spin_row1_);
    form_roi->addRow(QStringLiteral("Col1"), spin_col1_);
    form_roi->addRow(QStringLiteral("Row2"), spin_row2_);
    form_roi->addRow(QStringLiteral("Col2"), spin_col2_);
    form_roi->addRow(QString(), btn_draw);
    root->addWidget(group_roi_);

    // ── YOLO 检测 ─────────────────────────────────────────
    group_yolo_  = new QGroupBox(QStringLiteral("YOLO 检测"), this);
    auto *form_yolo = new QFormLayout(group_yolo_);

    check_yolo_  = new QCheckBox(QStringLiteral("启用 YOLO 检测"), this);
    edit_model_  = new QLineEdit(this);
    auto *btn_browse = new QPushButton(QStringLiteral("..."), this);
    btn_browse->setFixedWidth(30);
    connect(btn_browse, &QPushButton::clicked, this, &WorkflowConfigDialog::onBrowseModel);

    auto *model_row = new QHBoxLayout();
    model_row->addWidget(edit_model_);
    model_row->addWidget(btn_browse);

    spin_score_  = new QDoubleSpinBox(this); spin_score_->setRange(0.01, 1.0); spin_score_->setSingleStep(0.05);
    spin_nms_    = new QDoubleSpinBox(this); spin_nms_->setRange(0.01, 1.0);   spin_nms_->setSingleStep(0.05);
    combo_task_  = new QComboBox(this);
    combo_task_->addItems({"YOLO_DET", "YOLO_OBB"});
    check_e2e_   = new QCheckBox(QStringLiteral("End2End 模型"), this);

    form_yolo->addRow(QString(),                      check_yolo_);
    form_yolo->addRow(QStringLiteral("模型路径"),     model_row);
    form_yolo->addRow(QStringLiteral("置信度阈值"),   spin_score_);
    form_yolo->addRow(QStringLiteral("NMS 阈值"),     spin_nms_);
    form_yolo->addRow(QStringLiteral("任务类型"),     combo_task_);
    form_yolo->addRow(QString(),                      check_e2e_);
    root->addWidget(group_yolo_);

    // ── 按钮 ──────────────────────────────────────────────
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &WorkflowConfigDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void WorkflowConfigDialog::loadToUi()
{
    edit_name_->setText(QString::fromStdString(param_.name));
    check_enabled_->setChecked(param_.enabled);
    spin_di_addr_->setValue(param_.trigger_di_addr);
    spin_delay_ms_->setValue(param_.trigger_delay_ms);
    spin_do_ok_->setValue(param_.do_ok_addr);
    spin_do_ng_->setValue(param_.do_ng_addr);
    spin_exposure_->setValue(param_.exposure_time <= 0 ? -1.0 : param_.exposure_time);

    check_roi_->setChecked(param_.roi.enabled);
    spin_row1_->setValue(param_.roi.row1);
    spin_col1_->setValue(param_.roi.col1);
    spin_row2_->setValue(param_.roi.row2);
    spin_col2_->setValue(param_.roi.col2);

    check_yolo_->setChecked(param_.yolo.enabled);
    edit_model_->setText(QString::fromStdString(param_.yolo.model_path));
    spin_score_->setValue(param_.yolo.score_threshold);
    spin_nms_->setValue(param_.yolo.nms_threshold);
    combo_task_->setCurrentText(QString::fromStdString(param_.yolo.task_type));
    check_e2e_->setChecked(param_.yolo.end2end);
}

void WorkflowConfigDialog::saveFromUi()
{
    param_.name             = edit_name_->text().toStdString();
    param_.enabled          = check_enabled_->isChecked();
    param_.trigger_di_addr  = static_cast<uint16_t>(spin_di_addr_->value());
    param_.trigger_delay_ms = spin_delay_ms_->value();
    param_.do_ok_addr       = static_cast<uint16_t>(spin_do_ok_->value());
    param_.do_ng_addr       = static_cast<uint16_t>(spin_do_ng_->value());
    param_.exposure_time    = static_cast<float>(spin_exposure_->value());

    param_.roi.enabled = check_roi_->isChecked();
    param_.roi.row1    = spin_row1_->value();
    param_.roi.col1    = spin_col1_->value();
    param_.roi.row2    = spin_row2_->value();
    param_.roi.col2    = spin_col2_->value();

    param_.yolo.enabled          = check_yolo_->isChecked();
    param_.yolo.model_path       = edit_model_->text().toStdString();
    param_.yolo.score_threshold  = static_cast<float>(spin_score_->value());
    param_.yolo.nms_threshold    = static_cast<float>(spin_nms_->value());
    param_.yolo.task_type        = combo_task_->currentText().toStdString();
    param_.yolo.end2end          = check_e2e_->isChecked();
}

void WorkflowConfigDialog::onDrawRoi()
{
    if (!view_ || !view_->halconWindow())
    {
        spdlog::warn("WorkflowConfigDialog: no camera window for ROI drawing");
        return;
    }
    try
    {
        double r1, c1, r2, c2;
        view_->halconWindow()->SetColor("green");
        view_->halconWindow()->DrawRectangle1(&r1, &c1, &r2, &c2);
        spin_row1_->setValue(r1);
        spin_col1_->setValue(c1);
        spin_row2_->setValue(r2);
        spin_col2_->setValue(c2);
        check_roi_->setChecked(true);
    }
    catch (const HalconCpp::HException &e)
    {
        spdlog::error("WorkflowConfigDialog: DrawRectangle1 failed: {}", e.ErrorMessage().Text());
    }
}

void WorkflowConfigDialog::onBrowseModel()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择模型文件"), QString(),
        QStringLiteral("Model (*.onnx *.xml *.bin)"));
    if (!path.isEmpty())
        edit_model_->setText(path);
}

void WorkflowConfigDialog::accept()
{
    saveFromUi();
    QDialog::accept();
}
