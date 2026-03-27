#pragma once

#include <QDialog>
#include "../app/common.h"

class CameraViewWidget;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QGroupBox;

/// @brief 单条 WorkflowParam 的配置 Dialog
///        包含：基本参数 / ROI 裁剪 / YOLO 检测
class WorkflowConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WorkflowConfigDialog(WorkflowParam &param,
                                   CameraViewWidget *view,
                                   QWidget *parent = nullptr);

private slots:
    void onDrawRoi();
    void onBrowseModel();
    void accept() override;

private:
    void buildUi();
    void loadToUi();
    void saveFromUi();

    WorkflowParam &param_;
    CameraViewWidget *view_;

    // 基本参数
    QLineEdit    *edit_name_         = nullptr;
    QCheckBox    *check_enabled_     = nullptr;
    QSpinBox     *spin_di_addr_      = nullptr;
    QSpinBox     *spin_delay_ms_     = nullptr;
    QSpinBox     *spin_do_ok_        = nullptr;
    QSpinBox     *spin_do_ng_        = nullptr;
    QDoubleSpinBox *spin_exposure_   = nullptr;

    // ROI
    QGroupBox    *group_roi_         = nullptr;
    QCheckBox    *check_roi_         = nullptr;
    QDoubleSpinBox *spin_row1_       = nullptr;
    QDoubleSpinBox *spin_col1_       = nullptr;
    QDoubleSpinBox *spin_row2_       = nullptr;
    QDoubleSpinBox *spin_col2_       = nullptr;

    // YOLO
    QGroupBox    *group_yolo_        = nullptr;
    QCheckBox    *check_yolo_        = nullptr;
    QLineEdit    *edit_model_        = nullptr;
    QDoubleSpinBox *spin_score_      = nullptr;
    QDoubleSpinBox *spin_nms_        = nullptr;
    QComboBox    *combo_task_        = nullptr;
    QCheckBox    *check_e2e_         = nullptr;
};
