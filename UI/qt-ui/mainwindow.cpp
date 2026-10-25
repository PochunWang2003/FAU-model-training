#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "losschartwidget.h"

#include <QDateTime>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPixmap>
#include <QTableWidgetItem>

namespace {
const QStringList kAxes{"X", "Y", "Z", "RX", "RY", "RZ"};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("FAU Model Training");
    ui->motionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    const QStringList headers{"Axis", "Unit", "Minimum", "Maximum", "Excursion", "Max step", "Max speed", "Max load"};
    ui->motionTable->setHorizontalHeaderLabels(headers);
    for (int row = 0; row < kAxes.size(); ++row) {
        const QString axis = kAxes.at(row);
        const bool linear = axis.size() == 1;
        const QStringList values{axis, linear ? "mm" : "deg", linear ? "-5" : "-2", linear ? "5" : "2",
                                 linear ? "1" : "0.5", linear ? "0.2" : "0.1", linear ? "1" : "0.5", "1"};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column < 2) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->motionTable->setItem(row, column, item);
        }
    }
    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        ui->serverLabel->setText("Server: connected");
        appendLog("Connected to training server.");
        sendCommand("HELLO");
        sendCommand("GET_STATUS");
        sendCommand("MOTION_LIMIT_GET");
    });
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        ui->serverLabel->setText("Server: disconnected");
        appendLog("Server connection closed.");
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &MainWindow::readServer);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::connectServer);
    connect(ui->createFocusDatasetButton, &QPushButton::clicked, this, [this]() {
        m_focusDataset = ui->focusDatasetEdit->text().trimmed();
        sendCommand("DATASET_CREATE", {{"kind", "focus"}, {"dataset", m_focusDataset}, {"count", 20}});
    });
    connect(ui->labelFocusButton, &QPushButton::clicked, this, [this]() {
        sendCommand("DATASET_LABEL", {{"kind", "focus"}, {"dataset", ui->focusDatasetEdit->text().trimmed()},
                    {"image", ui->focusImageEdit->text().trimmed()}, {"label", ui->focusLabelCombo->currentText()}});
    });
    connect(ui->scoreFocusButton, &QPushButton::clicked, this, [this]() {
        sendCommand("FOCUS_SCORE", {{"dataset", ui->focusDatasetEdit->text().trimmed()}, {"image", ui->focusImageEdit->text().trimmed()}});
    });
    connect(ui->captureFocusButton, &QPushButton::clicked, this, [this]() {
        sendCommand("CAMERA_CAPTURE", {{"kind", "focus"}, {"dataset", ui->focusDatasetEdit->text().trimmed()}, {"role", "top"}});
    });
    connect(ui->importFocusButton, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileDialog::getExistingDirectory(this, "Import focus images");
        if (!folder.isEmpty()) sendCommand("DATASET_IMPORT", {{"kind", "focus"}, {"dataset", ui->focusDatasetEdit->text().trimmed()}, {"path", folder}});
    });
    connect(ui->startFocusTrainingButton, &QPushButton::clicked, this, [this]() {
        ui->focusLossChart->clear();
        ui->focusProgressBar->setValue(0);
        sendCommand("FOCUS_TRAIN_START", {{"dataset", ui->focusDatasetEdit->text().trimmed()},
                    {"epochs", ui->focusEpochSpin->value()}, {"learning_rate", ui->focusLearningRateSpin->value()},
                    {"device", "cpu"}, {"architecture", "statistical-logistic-regression"}});
    });
    connect(ui->cancelFocusTrainingButton, &QPushButton::clicked, this, [this]() { sendCommand("FOCUS_TRAIN_CANCEL"); });
    connect(ui->startTopButton, &QPushButton::clicked, this, [this]() { sendCommand("CAMERA_OPEN", {{"role", "top"}}); sendCommand("CAMERA_START", {{"role", "top"}}); });
    connect(ui->startSideButton, &QPushButton::clicked, this, [this]() { sendCommand("CAMERA_OPEN", {{"role", "side"}}); sendCommand("CAMERA_START", {{"role", "side"}}); });
    connect(ui->stopCameraButton, &QPushButton::clicked, this, [this]() { sendCommand("CAMERA_STOP"); });
    connect(ui->createFauDatasetButton, &QPushButton::clicked, this, [this]() {
        m_fauDataset = ui->fauDatasetEdit->text().trimmed();
        sendCommand("DATASET_CREATE", {{"kind", "fau"}, {"dataset", m_fauDataset}, {"count", 20}});
    });
    connect(ui->saveFauAnnotationButton, &QPushButton::clicked, this, [this]() {
        QJsonArray features{QJsonObject{{"id", ui->fauFeatureIdEdit->text().trimmed()}, {"x", ui->fauXSpin->value()},
                                       {"y", ui->fauYSpin->value()}, {"valid", true}, {"confidence", 1.0}}};
        QJsonObject roi{{"x", ui->roiXSpin->value()}, {"y", ui->roiYSpin->value()},
                        {"width", ui->roiWidthSpin->value()}, {"height", ui->roiHeightSpin->value()}};
        sendCommand("FAU_ANNOTATION_SAVE", {{"dataset", ui->fauDatasetEdit->text().trimmed()},
                    {"image", ui->fauImageEdit->text().trimmed()}, {"role", ui->fauRoleCombo->currentText()},
                    {"roi", roi}, {"features", features}});
    });
    connect(ui->startFauTrainingButton, &QPushButton::clicked, this, [this]() {
        sendCommand("FAU_TRAIN_START", {{"dataset", ui->fauDatasetEdit->text().trimmed()}, {"epochs", 10}, {"device", "cpu"}});
    });
    connect(ui->cancelFauTrainingButton, &QPushButton::clicked, this, [this]() { sendCommand("FAU_TRAIN_CANCEL"); });
    connect(ui->loadLimitsButton, &QPushButton::clicked, this, [this]() { sendCommand("MOTION_LIMIT_GET"); });
    connect(ui->validateLimitsButton, &QPushButton::clicked, this, [this]() { sendCommand("MOTION_LIMIT_VALIDATE", {{"limits", motionLimitsFromTable()}}); });
    connect(ui->saveLimitsButton, &QPushButton::clicked, this, [this]() { sendCommand("MOTION_LIMIT_SAVE", {{"limits", motionLimitsFromTable()}}); });
    connect(ui->simulateMotionButton, &QPushButton::clicked, this, [this]() {
        sendCommand("MOTION_SIMULATE", {{"axis", ui->motionAxisCombo->currentText()}, {"target", ui->motionTargetSpin->value()}, {"speed", ui->motionSpeedSpin->value()}});
    });
    connect(ui->refreshRunsButton, &QPushButton::clicked, this, [this]() { sendCommand("TRAINING_RUNS"); });
}

MainWindow::~MainWindow()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState) sendCommand("CAMERA_STOP");
    delete ui;
}

void MainWindow::connectServer()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.disconnectFromHost();
        return;
    }
    ui->serverLabel->setText("Server: connecting");
    m_socket.connectToHost(ui->hostEdit->text().trimmed(), static_cast<quint16>(ui->portSpin->value()));
}

void MainWindow::sendCommand(const QString &command, const QJsonObject &payload)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        appendLog("Not connected; command not sent: " + command);
        return;
    }
    const QString requestId = QString::number(++m_requestCounter);
    QJsonObject message{{"protocol_version", 1}, {"request_id", requestId}, {"type", "command"},
                        {"command", command}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"payload", payload}};
    m_socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void MainWindow::readServer()
{
    m_buffer += m_socket.readAll();
    while (true) {
        const qsizetype newline = m_buffer.indexOf('\n');
        if (newline < 0) break;
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (document.isObject()) handleMessage(document.object());
        else appendLog("Received an invalid server message.");
    }
}

void MainWindow::appendLog(const QString &text)
{
    const QString line = QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text;
    ui->logOutput->appendPlainText(line);
}

void MainWindow::handleMessage(const QJsonObject &message)
{
    const QString event = message.value("event").toString();
    const QJsonObject payload = message.value("payload").toObject();
    if (!message.value("ok").toBool(true)) {
        const QString error = message.value("error").toObject().value("message").toString("request failed");
        appendLog(event + " failed: " + error);
        ui->dashboardResults->appendPlainText(event + " failed: " + error);
        return;
    }
    if (event == "CAMERA_FRAME") {
        const QByteArray imageBytes = QByteArray::fromBase64(payload.value("image_base64").toString().toLatin1());
        QPixmap pixmap; pixmap.loadFromData(imageBytes, "PNG");
        QLabel *target = payload.value("role").toString() == "side" ? ui->previewSide : ui->previewTop;
        target->setPixmap(pixmap.scaled(target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }
    if (event == "TRAINING_PROGRESS") {
        const int epoch = payload.value("epoch").toInt();
        const int epochs = qMax(1, payload.value("epochs").toInt(1));
        ui->trainingStateLabel->setText(QString("Training: %1 %2/%3").arg(payload.value("task").toString()).arg(epoch).arg(epochs));
        ui->runStatusLabel->setText(payload.value("run_id").toString());
        ui->focusProgressBar->setMaximum(epochs); ui->focusProgressBar->setValue(epoch);
        const QString metrics = QString("Epoch: %1/%2 · Train loss: %3 · Validation loss: %4 · Accuracy: %5")
            .arg(epoch).arg(epochs).arg(payload.value("train_loss").toDouble(), 0, 'f', 4)
            .arg(payload.value("validation_loss").toDouble(), 0, 'f', 4)
            .arg(payload.value("accuracy").toDouble(-1.0) < 0 ? QStringLiteral("n/a") : QString::number(payload.value("accuracy").toDouble() * 100.0, 'f', 1) + "%");
        ui->focusMetricsLabel->setText(metrics);
        ui->focusLossChart->addPoint(payload.value("train_loss").toDouble(), payload.value("validation_loss").toDouble());
        ui->focusLog->appendPlainText(metrics);
        if (payload.contains("note")) ui->fauOutput->appendPlainText(payload.value("note").toString());
        return;
    }
    if (event == "TRAINING_STARTED") {
        ui->trainingStateLabel->setText("Training: preparing " + payload.value("task").toString());
        ui->runStatusLabel->setText(payload.value("run_id").toString());
    } else if (event == "TRAINING_COMPLETED") {
        ui->trainingStateLabel->setText("Training: completed");
        const QString summary = QJsonDocument(payload).toJson(QJsonDocument::Indented);
        ui->resultsOutput->appendPlainText(summary);
        ui->dashboardResults->appendPlainText(summary);
        ui->fauOutput->appendPlainText(summary);
        appendLog("Training run completed: " + payload.value("run_id").toString());
    } else if (event == "TRAINING_FAILED" || event == "TRAINING_CANCELLED") {
        ui->trainingStateLabel->setText("Training: " + event.mid(QStringLiteral("TRAINING_").size()).toLower());
        appendLog(event + ": " + payload.value("error").toString());
    } else if (event == "GET_STATUS" || event == "TRAINING_STATUS") {
        ui->trainingStateLabel->setText("Training: " + payload.value("training_state").toString());
        ui->cameraStatusLabel->setText(payload.value("camera_backend").toString());
        if (!payload.value("active_run_id").toString().isEmpty()) ui->runStatusLabel->setText(payload.value("active_run_id").toString());
    } else if (event == "MOTION_LIMIT_GET") {
        applyMotionLimits(payload);
    } else if (event == "MOTION_LIMIT_VALIDATE" || event == "MOTION_LIMIT_SAVE") {
        appendLog(event + (payload.value("saved").toBool() ? " saved and validated" : " passed server validation"));
    } else if (event == "MOTION_SIMULATE") {
        ui->simulatedPositionLabel->setText("Simulated position: " + QString::fromUtf8(QJsonDocument(payload.value("positions").toObject()).toJson(QJsonDocument::Compact)));
    } else if (event == "FOCUS_SCORE") {
        const QString score = QString::number(payload.value("score").toDouble(), 'g', 8);
        ui->focusLog->appendPlainText("Variance of Laplacian score: " + score + " (traditional baseline, not trained model)");
    } else if (event == "TRAINING_RUNS") {
        ui->resultsOutput->setPlainText(QString::fromUtf8(QJsonDocument(payload.value("runs").toArray()).toJson(QJsonDocument::Indented)));
    } else if (event == "FAU_ANNOTATION_SAVE") {
        ui->fauOutput->appendPlainText("Annotation saved with schema version " + QString::number(payload.value("schema_version").toInt()));
    } else if (event == "DATASET_CREATE") {
        appendLog("Created " + payload.value("kind").toString() + " dataset " + payload.value("dataset").toString());
        if (payload.value("kind").toString() == "focus") ui->datasetStatusLabel->setText(payload.value("dataset").toString() + " (synthetic demo)");
    } else if (event == "CAMERA_CAPTURE") {
        ui->focusImageEdit->setText(payload.value("image").toString());
        appendLog("Synthetic frame saved to dataset as " + payload.value("image").toString());
    } else if (event == "DATASET_IMPORT") {
        appendLog("Imported " + QString::number(payload.value("imported").toInt()) + " image(s) into " + payload.value("dataset").toString());
    } else if (event == "SERVER_LOG") {
        appendLog(payload.value("message").toString());
    } else {
        appendLog(event + " accepted");
    }
}

QJsonObject MainWindow::motionLimitsFromTable() const
{
    QJsonObject axes;
    for (int row = 0; row < kAxes.size(); ++row) {
        bool ok[6]{};
        const double minimum = ui->motionTable->item(row, 2)->text().toDouble(&ok[0]);
        const double maximum = ui->motionTable->item(row, 3)->text().toDouble(&ok[1]);
        const double excursion = ui->motionTable->item(row, 4)->text().toDouble(&ok[2]);
        const double step = ui->motionTable->item(row, 5)->text().toDouble(&ok[3]);
        const double speed = ui->motionTable->item(row, 6)->text().toDouble(&ok[4]);
        const double load = ui->motionTable->item(row, 7)->text().toDouble(&ok[5]);
        if (!(ok[0] && ok[1] && ok[2] && ok[3] && ok[4] && ok[5])) continue;
        axes.insert(kAxes.at(row), QJsonObject{{"unit", ui->motionTable->item(row, 1)->text()}, {"min", minimum}, {"max", maximum},
            {"max_excursion", excursion}, {"max_step", step}, {"max_speed", speed}, {"max_load", load}});
    }
    return {{"machine_profile", "DEMO_ONLY"}, {"simulation_only", true}, {"axes", axes}};
}

void MainWindow::applyMotionLimits(const QJsonObject &limits)
{
    const QJsonObject axes = limits.value("axes").toObject();
    for (int row = 0; row < kAxes.size(); ++row) {
        const QJsonObject values = axes.value(kAxes.at(row)).toObject();
        if (values.isEmpty()) continue;
        const QStringList text{QString::number(values.value("min").toDouble()), QString::number(values.value("max").toDouble()),
            QString::number(values.value("max_excursion").toDouble()), QString::number(values.value("max_step").toDouble()),
            QString::number(values.value("max_speed").toDouble()), QString::number(values.value("max_load").toDouble())};
        ui->motionTable->item(row, 1)->setText(values.value("unit").toString());
        for (int column = 0; column < text.size(); ++column) ui->motionTable->item(row, column + 2)->setText(text.at(column));
    }
}
