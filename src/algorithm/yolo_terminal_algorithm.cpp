#include "yolo_terminal_algorithm.h"
#include <spdlog/spdlog.h>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPushButton>

void YoloTerminalAlgorithm::configure(CameraViewWidget * /*view*/, QWidget *parent,
                                       nlohmann::json &params)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("YOLO 端子检测参数");
    dlg.setMinimumWidth(400);

    auto *layout = new QFormLayout(&dlg);

    // 模型路径
    auto *model_edit = new QLineEdit(QString::fromStdString(params.value("model_path", "")));
    auto *browse_btn = new QPushButton("...");
    browse_btn->setFixedWidth(30);
    auto *model_layout = new QHBoxLayout();
    model_layout->addWidget(model_edit);
    model_layout->addWidget(browse_btn);
    layout->addRow("模型路径", model_layout);

    QObject::connect(browse_btn, &QPushButton::clicked, [&]()
                     {
        QString path = QFileDialog::getOpenFileName(&dlg, "选择模型文件", "",
                                                     "Model (*.onnx *.xml *.bin)");
        if (!path.isEmpty()) model_edit->setText(path); });

    // 置信度阈值
    auto *score_spin = new QDoubleSpinBox();
    score_spin->setRange(0.01, 1.0);
    score_spin->setSingleStep(0.05);
    score_spin->setValue(params.value("score_threshold", 0.5));
    layout->addRow("置信度阈值", score_spin);

    // NMS 阈值
    auto *nms_spin = new QDoubleSpinBox();
    nms_spin->setRange(0.01, 1.0);
    nms_spin->setSingleStep(0.05);
    nms_spin->setValue(params.value("nms_threshold", 0.5));
    layout->addRow("NMS 阈值", nms_spin);

    // 任务类型
    auto *task_combo = new QComboBox();
    task_combo->addItem("YOLO_DET");
    task_combo->addItem("YOLO_OBB");
    task_combo->setCurrentText(QString::fromStdString(params.value("task_type", "YOLO_DET")));
    layout->addRow("任务类型", task_combo);

    // End2End
    auto *e2e_check = new QCheckBox();
    e2e_check->setChecked(params.value("end2end", false));
    layout->addRow("End2End 模型", e2e_check);

    // OK / Cancel
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // 写回参数
    params["model_path"] = model_edit->text().toStdString();
    params["score_threshold"] = score_spin->value();
    params["nms_threshold"] = nms_spin->value();
    params["task_type"] = task_combo->currentText().toStdString();
    params["end2end"] = e2e_check->isChecked();

    params_ = params;
    initDetector();
}

void YoloTerminalAlgorithm::loadParams(const nlohmann::json &params)
{
    params_ = params;
    if (params_.contains("model_path") && !params_["model_path"].get<std::string>().empty())
        initDetector();
}

void YoloTerminalAlgorithm::initDetector()
{
    try
    {
        AIInfer::YOLOSettings settings;
        settings.model_path = params_.value("model_path", "");
        settings.score_threshold = params_.value("score_threshold", 0.5f);
        settings.nms_threshold = params_.value("nms_threshold", 0.5f);
        settings.image_stride = params_.value("image_stride", 32);
        settings.end2end = params_.value("end2end", false);

        std::string task = params_.value("task_type", "YOLO_DET");
        settings.task_type = (task == "YOLO_OBB") ? AIInfer::TaskType::YOLO_OBB
                                                   : AIInfer::TaskType::YOLO_DET;
        settings.input_type = AIInfer::InputDimensionType::DYNAMIC;
        settings.engine_type = AIInfer::EngineType::OPENVINO;

        detector_ = std::make_unique<AIInfer::YoloDetector>(settings);
        spdlog::info("YoloTerminal detector initialized: {}", settings.model_path);
    }
    catch (const std::exception &e)
    {
        spdlog::error("YoloTerminal initDetector failed: {}", e.what());
        detector_.reset();
    }
}

bool YoloTerminalAlgorithm::process(NodeContext &ctx)
{
    if (!detector_)
    {
        ctx.result.pass = false;
        ctx.result.detail += "detector not initialized; ";
        return false;
    }

    try
    {
        auto det_result = detector_->detect(ctx.image);

        bool has_detections = std::visit([](const auto &dets) { return !dets.empty(); }, det_result);

        if (!has_detections)
        {
            ctx.result.pass = false;
            ctx.result.detail += "no object detected; ";
            ctx.result.confidence = 0.0;
            return true;
        }

        double max_conf = 0.0;
        int count = 0;
        std::visit([&](const auto &dets)
                   {
                       count = static_cast<int>(dets.size());
                       for (const auto &d : dets)
                       {
                           double c;
                           if constexpr (std::is_same_v<std::decay_t<decltype(d)>, AIInfer::Detection>)
                               c = d.conf;
                           else
                               c = d.detection.conf;
                           if (c > max_conf)
                               max_conf = c;
                       }
                   },
                   det_result);

        ctx.result.confidence = max_conf;
        ctx.result.detail += "detected " + std::to_string(count) + " objects; ";
        return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("YoloTerminal process failed: {}", e.what());
        ctx.result.pass = false;
        ctx.result.detail += std::string("exception: ") + e.what() + "; ";
        return false;
    }
}
