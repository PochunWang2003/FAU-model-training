#include "trainingserver.h"
#include "motion/SimulatedMotionFeedbackProvider.h"

#include <QDateTime>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <QtMath>

#include <cmath>

namespace {
constexpr int kProtocolVersion = 1;
constexpr int kMaximumMessageBytes = 1024 * 1024;
constexpr int kMaximumDatasetImages = 2000;
}

TrainingServer::TrainingServer(const QString &projectRoot, QObject *parent)
    : QObject(parent), m_projectRoot(QFileInfo(projectRoot).absoluteFilePath())
{
    m_workerScript = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../ML/workers/train.py");
    auto simulatedProvider = std::make_unique<SimulatedMotionFeedbackProvider>();
    m_simulatedMotionFeedback = simulatedProvider.get();
    m_motionFeedback = std::move(simulatedProvider);
    QDir().mkpath(m_projectRoot + "/data/focus");
    QDir().mkpath(m_projectRoot + "/data/fau");
    QDir().mkpath(m_projectRoot + "/models/focus");
    QDir().mkpath(m_projectRoot + "/models/fau");
    QDir().mkpath(m_projectRoot + "/runs");
    const QString limitsPath = m_projectRoot + "/config/motion_limits.json";
    QFile limitsFile(limitsPath);
    if (limitsFile.open(QIODevice::ReadOnly)) {
        m_limits = QJsonDocument::fromJson(limitsFile.readAll()).object();
    }
    if (validateLimits(m_limits).size() > 0) {
        m_limits = defaultLimits();
    }
    m_previewTimer.setInterval(400);
    connect(&m_previewTimer, &QTimer::timeout, this, &TrainingServer::sendPreviewFrames);
    connect(&m_listener, &QTcpServer::newConnection, this, &TrainingServer::acceptConnections);
}

bool TrainingServer::listen(const QHostAddress &address, quint16 port)
{
    return m_listener.listen(address, port);
}

void TrainingServer::acceptConnections()
{
    while (m_listener.hasPendingConnections()) {
        QTcpSocket *socket = m_listener.nextPendingConnection();
        m_buffers.insert(socket, {});
        m_seenRequests.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, &TrainingServer::readClient);
        connect(socket, &QTcpSocket::disconnected, this, &TrainingServer::removeClient);
    }
}

void TrainingServer::removeClient()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;
    m_buffers.remove(socket);
    m_seenRequests.remove(socket);
    m_streamingRoles.remove(socket);
    if (m_jobClient == socket) m_jobClient = nullptr;
    socket->deleteLater();
}

void TrainingServer::readClient()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaximumMessageBytes && !buffer.contains('\n')) {
        socket->disconnectFromHost();
        return;
    }
    while (true) {
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0) break;
        if (newline > kMaximumMessageBytes) {
            socket->disconnectFromHost();
            return;
        }
        const QByteArray line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            respond(socket, {}, "ERROR", {}, false, "Malformed JSON command");
            continue;
        }
        processMessage(socket, document.object());
    }
}

void TrainingServer::respond(QTcpSocket *socket, const QString &requestId, const QString &event,
                             const QJsonObject &payload, bool ok, const QString &error)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    QJsonObject message{{"protocol_version", kProtocolVersion}, {"type", "response"},
                        {"event", event}, {"request_id", requestId}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"ok", ok}, {"payload", payload}};
    if (!error.isEmpty()) message.insert("error", QJsonObject{{"code", "INVALID_REQUEST"}, {"message", error}});
    if (socket->bytesToWrite() > 2 * kMaximumMessageBytes) { socket->disconnectFromHost(); return; }
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void TrainingServer::emitEvent(QTcpSocket *socket, const QString &event, const QJsonObject &payload)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    QJsonObject message{{"protocol_version", kProtocolVersion}, {"type", "event"}, {"event", event},
                        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"payload", payload}};
    if (socket->bytesToWrite() > 2 * kMaximumMessageBytes) { socket->disconnectFromHost(); return; }
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

bool TrainingServer::validId(const QString &value) const
{
    return QRegularExpression("^[A-Za-z0-9_-]{1,48}$").match(value).hasMatch();
}

QString TrainingServer::datasetPath(const QString &kind, const QString &id) const
{
    return m_projectRoot + "/data/" + kind + "/" + id;
}

QImage TrainingServer::syntheticFrame(const QString &role, int seed, bool focusDemo) const
{
    const int width = 160;
    const int height = 120;
    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            int value = 20 + ((x * 3 + y * 5 + seed * 11) % 40);
            if ((x / 8 + y / 8 + seed) % 2 == 0) value += focusDemo ? 170 : 90;
            value += (role == "side" ? (x / 4) : (y / 4)) % 20;
            line[x] = static_cast<uchar>(qBound(0, value, 255));
        }
    }
    return image;
}

bool TrainingServer::writePgm(const QImage &image, const QString &path) const
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QString("P5\n%1 %2\n255\n").arg(gray.width()).arg(gray.height()).toLatin1());
    for (int y = 0; y < gray.height(); ++y) file.write(reinterpret_cast<const char *>(gray.constScanLine(y)), gray.width());
    return file.flush();
}

bool TrainingServer::createDataset(const QString &kind, const QString &id, int count, QString *error)
{
    if ((kind != "focus" && kind != "fau") || !validId(id) || count < 4 || count > kMaximumDatasetImages) {
        *error = "dataset kind, id, or sample count is invalid";
        return false;
    }
    const QString path = datasetPath(kind, id);
    if (QFileInfo::exists(path)) {
        *error = "dataset already exists";
        return false;
    }
    QDir().mkpath(path);
    QFile manifest(path + "/manifest.jsonl");
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Text)) {
        *error = "cannot create dataset manifest";
        return false;
    }
    for (int i = 0; i < count; ++i) {
        const QString filename = QString("sample_%1.pgm").arg(i, 4, 10, QLatin1Char('0'));
        QImage sample = syntheticFrame(i % 2 ? "top" : "side", i, true);
        if (i % 2 != 0) {
            const QImage original = sample;
            for (int y = 1; y < sample.height() - 1; ++y) for (int x = 1; x < sample.width() - 1; ++x) {
                int sum = 0;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) sum += original.constScanLine(y + dy)[x + dx];
                sample.scanLine(y)[x] = static_cast<uchar>(sum / 9);
            }
        }
        if (!writePgm(sample, path + "/" + filename)) {
            *error = "cannot write synthetic image";
            return false;
        }
        const QString label = i % 2 == 0 ? "sharp" : "blurred";
        const QString split = i < (count * 4 / 5) ? "train" : "validation";
        QJsonObject row{{"image", filename}, {"role", i % 2 ? "side" : "top"},
                        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"label", kind == "focus" ? QJsonValue(label) : QJsonValue(QJsonValue::Null)},
                        {"split", split}, {"synthetic_demo_only", true}, {"schema_version", 1}};
        manifest.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n');
    }
    return true;
}

QJsonObject TrainingServer::status() const
{
    return {{"server", "ready"}, {"port", m_listener.serverPort()}, {"camera_backend", "synthetic"},
            {"training_state", m_worker ? "RUNNING" : "IDLE"}, {"active_run_id", m_activeRunId},
            {"active_task", m_activeTask}, {"motion_mode", "simulation-only"},
            {"motion_feedback", m_motionFeedback->ReadSnapshot().ToJson()}};
}

QJsonObject TrainingServer::poseMap() const
{
    const MotionFeedbackSnapshot snapshot = m_motionFeedback->ReadSnapshot();
    const QStringList axes{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                           QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")};
    QJsonObject values;
    for (qsizetype index = 0; index < axes.size(); ++index) values.insert(axes.at(index), snapshot.pose[static_cast<size_t>(index)]);
    return values;
}

QJsonObject TrainingServer::defaultLimits() const
{
    QJsonObject axes;
    for (const QString &axis : QStringList{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"), QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")}) {
        const bool linear = axis.size() == 1;
        axes.insert(axis, QJsonObject{{"unit", linear ? "mm" : "deg"},
            {"min", linear ? -5.0 : -2.0}, {"max", linear ? 5.0 : 2.0},
            {"max_excursion", linear ? 1.0 : 0.5}, {"max_step", linear ? 0.2 : 0.1},
            {"max_speed", linear ? 1.0 : 0.5}, {"max_load", 1.0}});
    }
    return {{"machine_profile", "DEMO_ONLY"}, {"simulation_only", true}, {"axes", axes}};
}

QString TrainingServer::validateLimits(const QJsonObject &limits) const
{
    const QJsonObject axes = limits.value("axes").toObject();
    if (limits.value("machine_profile").toString().trimmed().isEmpty() || !limits.value("simulation_only").toBool() || axes.size() != 6)
        return "machine profile, simulation-only flag, and six axis definitions are required";
    for (const QString &axis : QStringList{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"), QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")}) {
        const QJsonObject entry = axes.value(axis).toObject();
        const QString expectedUnit = axis.size() == 1 ? "mm" : "deg";
        if (entry.value("unit").toString() != expectedUnit) return axis + " unit must be " + expectedUnit;
        for (const QString &key : QStringList{QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("max_excursion"), QStringLiteral("max_step"), QStringLiteral("max_speed"), QStringLiteral("max_load")}) {
            if (!entry.value(key).isDouble() || !std::isfinite(entry.value(key).toDouble())) return axis + " has a missing or non-finite " + key;
        }
        if (entry.value("min").toDouble() >= entry.value("max").toDouble()) return axis + " minimum must be less than maximum";
        for (const QString &key : QStringList{QStringLiteral("max_excursion"), QStringLiteral("max_step"), QStringLiteral("max_speed"), QStringLiteral("max_load")})
            if (entry.value(key).toDouble() <= 0.0) return axis + " " + key + " must be positive";
        if (entry.value("max_excursion").toDouble() > entry.value("max").toDouble() - entry.value("min").toDouble())
            return axis + " maximum excursion exceeds the configured position range";
    }
    return {};
}

QString TrainingServer::validateMotion(const QJsonObject &request) const
{
    const QString axis = request.value("axis").toString();
    const QStringList axes{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                           QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")};
    if (!axes.contains(axis)) return "unknown axis";
    if (!request.value("target").isDouble() || !request.value("speed").isDouble()) return "target and speed must be finite numbers";
    const double target = request.value("target").toDouble();
    const double speed = request.value("speed").toDouble();
    if (!std::isfinite(target) || !std::isfinite(speed)) return "target and speed must be finite numbers";
    const QJsonObject limits = m_limits.value("axes").toObject().value(axis).toObject();
    const double current = m_motionFeedback->Position(axis);
    if (target < limits.value("min").toDouble() || target > limits.value("max").toDouble()) return "target is outside the configured position envelope";
    if (std::abs(target - current) > limits.value("max_excursion").toDouble()) return "requested excursion exceeds the configured maximum";
    if (std::abs(target - current) > limits.value("max_step").toDouble()) return "requested step exceeds the configured maximum";
    if (speed <= 0.0 || speed > limits.value("max_speed").toDouble()) return "requested speed is outside the configured range";
    const double load = request.value("load").toDouble(0.0);
    if (!std::isfinite(load) || load < 0.0 || load > limits.value("max_load").toDouble()) return "requested load exceeds the configured maximum";
    return {};
}

void TrainingServer::processMessage(QTcpSocket *socket, const QJsonObject &message)
{
    const QString requestId = message.value("request_id").toString();
    if (message.value("protocol_version").toInt() != kProtocolVersion || message.value("type").toString() != "command") {
        respond(socket, requestId, "ERROR", {}, false, "Unsupported protocol version or message type");
        return;
    }
    const QString command = message.value("command").toString();
    const QJsonObject payload = message.value("payload").toObject();
    if (command.isEmpty() || requestId.isEmpty() || requestId.size() > 128 || command.size() > 64) {
        respond(socket, requestId, "ERROR", {}, false, "command and request_id are required");
        return;
    }
    QSet<QString> &seen = m_seenRequests[socket];
    if (seen.contains(requestId)) { respond(socket, requestId, "ERROR", {}, false, "duplicate request_id; command was not repeated"); return; }
    if (seen.size() >= 4096) { socket->disconnectFromHost(); return; }
    seen.insert(requestId);
    if (command == "HELLO") { respond(socket, requestId, "HELLO", {{"protocol_version", kProtocolVersion}, {"server", "FAU Model Training"}}); return; }
    if (command == "GET_STATUS" || command == "TRAINING_STATUS") {
        if (m_worker && (!m_jobClient || m_jobClient->state() != QAbstractSocket::ConnectedState)) m_jobClient = socket;
        respond(socket, requestId, command, status()); return;
    }
    if (command == "CAMERA_LIST") {
        respond(socket, requestId, command, {{"cameras", QJsonArray{QJsonObject{{"id","synthetic-0"},{"role","top"},{"state","available"}}, QJsonObject{{"id","synthetic-1"},{"role","side"},{"state","available"}}}}}); return;
    }
    if (command == "CAMERA_OPEN" || command == "CAMERA_CLOSE") { respond(socket, requestId, command, {{"state", command == "CAMERA_OPEN" ? "open" : "closed"}, {"backend", "synthetic"}}); return; }
    if (command == "CAMERA_START") {
        const QString role = payload.value("role").toString("top");
        if (role != "top" && role != "side") { respond(socket, requestId, command, {}, false, "role must be top or side"); return; }
        m_streamingRoles[socket].insert(role);
        if (!m_previewTimer.isActive()) m_previewTimer.start();
        respond(socket, requestId, command, {{"state", "streaming"}, {"role", role}}); return;
    }
    if (command == "CAMERA_STOP") {
        m_streamingRoles[socket].clear();
        if (m_streamingRoles.isEmpty()) m_previewTimer.stop();
        respond(socket, requestId, command, {{"state", "stopped"}}); return;
    }
    if (command == "CAMERA_CAPTURE") {
        const QString kind = payload.value("kind").toString("focus");
        const QString id = payload.value("dataset").toString();
        const QString role = payload.value("role").toString("top");
        const QString path = datasetPath(kind, id);
        if (!validId(id) || (kind != "focus" && kind != "fau") || (role != "top" && role != "side") || !QFileInfo(path).isDir()) { respond(socket, requestId, command, {}, false, "dataset, kind, or role is invalid"); return; }
        const QString filename = QString("capture_%1.pgm").arg(QDateTime::currentMSecsSinceEpoch());
        if (!writePgm(syntheticFrame(role, static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000), true), path + "/" + filename)) { respond(socket, requestId, command, {}, false, "capture could not be saved"); return; }
        QFile manifest(path + "/manifest.jsonl");
        if (manifest.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject row{{"image", filename}, {"role", role}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"label", QJsonValue(QJsonValue::Null)}, {"split", "train"}, {"synthetic_demo_only", true}, {"schema_version", 1}};
            manifest.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n');
        }
        respond(socket, requestId, command, {{"image", filename}, {"dataset", id}, {"role", role}}); return;
    }
    if (command == "DATASET_CREATE") {
        QString error;
        const QString kind = payload.value("kind").toString("focus");
        const QString id = payload.value("dataset").toString();
        if (!createDataset(kind, id, payload.value("count").toInt(20), &error)) { respond(socket, requestId, command, {}, false, error); return; }
        respond(socket, requestId, command, {{"dataset", id}, {"kind", kind}, {"count", payload.value("count").toInt(20)}, {"demonstration_only", true}}); return;
    }
    if (command == "DATASET_IMPORT") {
        const QString kind = payload.value("kind").toString("focus");
        const QString id = payload.value("dataset").toString();
        const QString source = payload.value("path").toString();
        const QString target = datasetPath(kind, id);
        if ((kind != "focus" && kind != "fau") || !validId(id) || !QFileInfo(target).isDir() || !QFileInfo(source).isDir()) {
            respond(socket, requestId, command, {}, false, "existing dataset and source image folder are required"); return;
        }
        const QStringList files = QDir(source).entryList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.pgm", "*.tif", "*.tiff"}, QDir::Files, QDir::Name);
        QFile manifest(target + "/manifest.jsonl");
        if (files.isEmpty() || !manifest.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) { respond(socket, requestId, command, {}, false, "no supported images found or manifest is not writable"); return; }
        int imported = 0;
        for (const QString &fileName : files) {
            if (imported >= kMaximumDatasetImages) break;
            const QImage image(QDir(source).filePath(fileName));
            if (image.isNull()) continue;
            const QString destinationName = QString("import_%1.pgm").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            if (!writePgm(image, target + "/" + destinationName)) continue;
            QJsonObject row{{"image", destinationName}, {"role", "top"}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                            {"label", QJsonValue(QJsonValue::Null)}, {"split", "train"}, {"source_name", fileName}, {"schema_version", 1}};
            manifest.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n');
            ++imported;
        }
        respond(socket, requestId, command, {{"imported", imported}, {"kind", kind}, {"dataset", id}}); return;
    }
    if (command == "DATASET_LIST") {
        const QString kind = payload.value("kind").toString("focus");
        if (kind != "focus" && kind != "fau") { respond(socket, requestId, command, {}, false, "kind must be focus or fau"); return; }
        QStringList ids = QDir(m_projectRoot + "/data/" + kind).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QJsonArray array; for (const QString &id : ids) array.append(id);
        respond(socket, requestId, command, {{"datasets", array}}); return;
    }
    if (command == "DATASET_LABEL" || command == "DATASET_SPLIT") {
        const QString kind = payload.value("kind").toString("focus"), id = payload.value("dataset").toString();
        const QString image = QFileInfo(payload.value("image").toString()).fileName();
        if ((kind != "focus" && kind != "fau") || !validId(id) || image.isEmpty()) { respond(socket, requestId, command, {}, false, "dataset kind, ID, and image filename are required"); return; }
        const QString path = datasetPath(kind, id) + "/manifest.jsonl";
        QFile manifest(path);
        if (!manifest.open(QIODevice::ReadOnly | QIODevice::Text)) { respond(socket, requestId, command, {}, false, "dataset manifest not found"); return; }
        QList<QJsonObject> rows;
        while (!manifest.atEnd()) { const auto row = QJsonDocument::fromJson(manifest.readLine()).object(); if (!row.isEmpty()) rows.append(row); }
        manifest.close();
        bool found = false;
        for (QJsonObject &row : rows) if (row.value("image").toString() == image) {
            if (command == "DATASET_LABEL") {
                const QString label = payload.value("label").toString();
                if (kind != "focus") { respond(socket, requestId, command, {}, false, "focus labels can only be assigned to focus datasets"); return; }
                if (label != "sharp" && label != "blurred") { respond(socket, requestId, command, {}, false, "focus label must be sharp or blurred"); return; }
                row.insert("label", label);
            } else {
                const QString split = payload.value("split").toString();
                if (split != "train" && split != "validation" && split != "test") { respond(socket, requestId, command, {}, false, "split must be train, validation, or test"); return; }
                row.insert("split", split);
            }
            found = true;
        }
        if (!found) { respond(socket, requestId, command, {}, false, "image is not in the dataset"); return; }
        if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { respond(socket, requestId, command, {}, false, "cannot update manifest"); return; }
        for (const auto &row : rows) manifest.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n');
        respond(socket, requestId, command, {{"image", image}, {"updated", true}}); return;
    }
    if (command == "FOCUS_SCORE") {
        const QString id = payload.value("dataset").toString();
        const QString imageName = QFileInfo(payload.value("image").toString()).fileName();
        if (!validId(id) || imageName.isEmpty()) { respond(socket, requestId, command, {}, false, "focus dataset ID and image filename are required"); return; }
        const QString path = datasetPath("focus", id) + "/" + imageName;
        QImage image(path);
        if (image.isNull()) { respond(socket, requestId, command, {}, false, "image could not be loaded"); return; }
        const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
        QVector<double> values;
        for (int y = 1; y < gray.height() - 1; ++y) for (int x = 1; x < gray.width() - 1; ++x) {
            const int lap = gray.constScanLine(y)[x - 1] + gray.constScanLine(y)[x + 1] + gray.constScanLine(y - 1)[x] + gray.constScanLine(y + 1)[x] - 4 * gray.constScanLine(y)[x];
            values.append(lap);
        }
        double mean = 0; for (double value : values) mean += value; mean /= qMax(1, values.size());
        double variance = 0; for (double value : values) variance += (value - mean) * (value - mean); variance /= qMax(1, values.size());
        respond(socket, requestId, command, {{"algorithm", "variance-of-laplacian"}, {"score", variance}, {"is_trained_model", false}}); return;
    }
    if (command == "FAU_ANNOTATION_SAVE") {
        const QString id = payload.value("dataset").toString();
        const QString filename = QFileInfo(payload.value("image").toString()).fileName();
        const QString path = datasetPath("fau", id);
        const QString role = payload.value("role").toString("top");
        const QImage image(path + "/" + filename);
        const QJsonObject roi = payload.value("roi").toObject();
        const QJsonArray features = payload.value("features").toArray();
        if (!validId(id) || filename.isEmpty() || image.isNull() || (role != "top" && role != "side")) { respond(socket, requestId, command, {}, false, "FAU dataset, image, or role is invalid"); return; }
        if (roi.value("x").toInt(-1) < 0 || roi.value("y").toInt(-1) < 0 || roi.value("width").toInt() <= 0 || roi.value("height").toInt() <= 0 ||
            roi.value("x").toInt() + roi.value("width").toInt() > image.width() || roi.value("y").toInt() + roi.value("height").toInt() > image.height()) {
            respond(socket, requestId, command, {}, false, "ROI must be a positive rectangle inside the image"); return;
        }
        for (const QJsonValue &featureValue : features) {
            const QJsonObject feature = featureValue.toObject();
            if (feature.value("id").toString().trimmed().isEmpty() || !feature.value("x").isDouble() || !feature.value("y").isDouble() ||
                !std::isfinite(feature.value("x").toDouble()) || !std::isfinite(feature.value("y").toDouble()) ||
                feature.value("x").toDouble() < roi.value("x").toInt() || feature.value("y").toDouble() < roi.value("y").toInt() ||
                feature.value("x").toDouble() >= roi.value("x").toInt() + roi.value("width").toInt() ||
                feature.value("y").toDouble() >= roi.value("y").toInt() + roi.value("height").toInt()) {
                respond(socket, requestId, command, {}, false, "feature keypoints must be finite and inside the ROI"); return;
            }
            const double confidence = feature.value("confidence").toDouble(1.0);
            if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) { respond(socket, requestId, command, {}, false, "feature confidence must be in [0, 1]"); return; }
        }
        QJsonObject annotation{{"image", filename}, {"role", role},
                               {"roi", roi}, {"features", features},
                               {"image_width", image.width()}, {"image_height", image.height()},
                               {"schema_version", 1}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        QFile file(path + "/annotations.jsonl");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) { respond(socket, requestId, command, {}, false, "cannot save annotation"); return; }
        file.write(QJsonDocument(annotation).toJson(QJsonDocument::Compact) + '\n');
        respond(socket, requestId, command, {{"saved", true}, {"schema_version", 1}}); return;
    }
    if (command == "FOCUS_INFER" || command == "FAU_INFER") {
        startInference(socket, requestId, command == "FOCUS_INFER" ? "focus" : "fau", payload);
        return;
    }
    if (command == "FOCUS_TRAIN_START" || command == "FAU_TRAIN_START") { startTraining(socket, requestId, command.startsWith("FOCUS") ? "focus" : "fau", payload); return; }
    if (command == "FOCUS_TRAIN_CANCEL" || command == "FAU_TRAIN_CANCEL") {
        if (!m_worker) { respond(socket, requestId, command, {}, false, "no training job is active"); return; }
        QFile cancelFile(m_activeRunDir + "/cancel"); cancelFile.open(QIODevice::WriteOnly); cancelFile.close();
        m_cancelRequested = true;
        respond(socket, requestId, command, {{"run_id", m_activeRunId}, {"state", "CANCELLING"}}); return;
    }
    if (command == "MOTION_LIMIT_GET") { respond(socket, requestId, command, m_limits); return; }
    if (command == "MOTION_FEEDBACK_GET") { respond(socket, requestId, command, m_motionFeedback->ReadSnapshot().ToJson()); return; }
    if (command == "MOTION_LIMIT_VALIDATE" || command == "MOTION_LIMIT_SAVE") {
        const QJsonObject proposed = payload.value("limits").toObject();
        const QString error = validateLimits(proposed);
        if (!error.isEmpty()) { respond(socket, requestId, command, {}, false, error); return; }
        if (command == "MOTION_LIMIT_SAVE") {
            QDir().mkpath(m_projectRoot + "/config"); QFile file(m_projectRoot + "/config/motion_limits.json");
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { respond(socket, requestId, command, {}, false, "cannot persist motion limits"); return; }
            file.write(QJsonDocument(proposed).toJson(QJsonDocument::Indented)); m_limits = proposed;
        }
        respond(socket, requestId, command, {{"valid", true}, {"saved", command == "MOTION_LIMIT_SAVE"}}); return;
    }
    if (command == "MOTION_SIMULATE") {
        const QString error = validateMotion(payload);
        if (!error.isEmpty()) { respond(socket, requestId, command, {}, false, error); return; }
        m_simulatedMotionFeedback->SetSimulatedPosition(payload.value("axis").toString(), payload.value("target").toDouble());
        respond(socket, requestId, command, {{"simulation_only", true}, {"positions", poseMap()},
            {"motion_feedback", m_motionFeedback->ReadSnapshot().ToJson()}}); return;
    }
    if (command == "TRAINING_RUNS") {
        QJsonArray runs; QDir dir(m_projectRoot + "/runs");
        for (const QString &id : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile result(dir.filePath(id + "/result.json")); if (result.open(QIODevice::ReadOnly)) runs.append(QJsonDocument::fromJson(result.readAll()).object());
        }
        respond(socket, requestId, command, {{"runs", runs}}); return;
    }
    if (command == "TRAINING_RESULT" || command == "FOCUS_EVALUATE" || command == "FAU_EVALUATE") {
        const QString runId = payload.value("run_id").toString();
        if (!validId(runId)) { respond(socket, requestId, command, {}, false, "valid run_id is required"); return; }
        QFile result(m_projectRoot + "/runs/" + runId + "/result.json");
        if (!result.open(QIODevice::ReadOnly)) { respond(socket, requestId, command, {}, false, "saved result for this run is unavailable"); return; }
        const QJsonObject record = QJsonDocument::fromJson(result.readAll()).object();
        if ((command == "FOCUS_EVALUATE" && !record.value("task").toString().startsWith("focus")) ||
            (command == "FAU_EVALUATE" && !record.value("task").toString().startsWith("fau"))) {
            respond(socket, requestId, command, {}, false, "run task does not match the requested evaluation"); return;
        }
        respond(socket, requestId, command, record); return;
    }
    respond(socket, requestId, "ERROR", {}, false, "unknown command: " + command);
}

void TrainingServer::sendPreviewFrames()
{
    for (auto it = m_streamingRoles.begin(); it != m_streamingRoles.end(); ++it) {
        QTcpSocket *socket = it.key();
        for (const QString &role : it.value()) {
            const QImage image = syntheticFrame(role, static_cast<int>(QDateTime::currentMSecsSinceEpoch() / 400), false);
            QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
            emitEvent(socket, "CAMERA_FRAME", {{"role", role}, {"width", image.width()}, {"height", image.height()}, {"format", "PNG"}, {"image_base64", QString::fromLatin1(bytes.toBase64())}});
        }
    }
}

void TrainingServer::startInference(QTcpSocket *socket, const QString &requestId, const QString &task,
                                   const QJsonObject &payload)
{
    if (m_worker || m_inference) { respond(socket, requestId, task == "focus" ? "FOCUS_INFER" : "FAU_INFER", {}, false, "inference is unavailable while another worker is active"); return; }
    const QString dataset = payload.value("dataset").toString();
    const QString imageName = QFileInfo(payload.value("image").toString()).fileName();
    const QString role = payload.value("role").toString("top");
    const QString checkpointName = payload.value("checkpoint").toString();
    if (!validId(dataset) || imageName.isEmpty() || (task == "fau" && role != "top" && role != "side") ||
        QFileInfo(checkpointName).fileName() != checkpointName || (QFileInfo(checkpointName).suffix() != "pt" && QFileInfo(checkpointName).suffix() != "json")) {
        respond(socket, requestId, task == "focus" ? "FOCUS_INFER" : "FAU_INFER", {}, false, "dataset, image, role, or checkpoint name is invalid"); return;
    }
    const QString imagePath = datasetPath(task, dataset) + "/" + imageName;
    const QString checkpointPath = m_projectRoot + "/models/" + task + "/" + checkpointName;
    if (!QFileInfo(imagePath).isFile() || !QFileInfo(checkpointPath).isFile()) {
        respond(socket, requestId, task == "focus" ? "FOCUS_INFER" : "FAU_INFER", {}, false, "selected dataset image or checkpoint does not exist"); return;
    }
    m_inferenceRequestId = requestId;
    m_inferenceCommand = task == "focus" ? "FOCUS_INFER" : "FAU_INFER";
    m_inferenceImagePath = imagePath;
    m_inferenceClient = socket;
    m_inferenceOutput.clear();
    auto *process = new QProcess(this);
    m_inference = process;
    process->setProgram("python3");
    process->setArguments({QDir(QFileInfo(m_workerScript).absolutePath()).filePath("infer.py"),
                           "--task", task, "--checkpoint", checkpointPath, "--image", imagePath,
                           "--role", role});
    connect(process, &QProcess::readyReadStandardOutput, this, &TrainingServer::readInferenceOutput);
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        const QString text = QString::fromUtf8(process->readAllStandardError()).trimmed();
        if (!text.isEmpty()) emitEvent(m_inferenceClient, "SERVER_LOG", {{"level", "warning"}, {"message", text}});
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &TrainingServer::finishInference);
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finishInference(-1, QProcess::CrashExit);
    });
    process->start();
}

void TrainingServer::readInferenceOutput()
{
    if (m_inference) m_inferenceOutput += QString::fromUtf8(m_inference->readAllStandardOutput());
}

void TrainingServer::finishInference(int exitCode, QProcess::ExitStatus exitStatus)
{
    readInferenceOutput();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_inferenceOutput.toUtf8().trimmed(), &parseError);
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject payload = document.object();
        const QString event = payload.take("event").toString();
        if (event == "INFERENCE_RESULT") {
            QImage original(m_inferenceImagePath);
            const QSize originalSize = original.size();
            const QImage preview = original.scaled(QSize(512, 512), Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
            QByteArray imageBytes; QBuffer imageBuffer(&imageBytes); imageBuffer.open(QIODevice::WriteOnly); preview.save(&imageBuffer, "PNG");
            payload.insert("image_base64", QString::fromLatin1(imageBytes.toBase64()));
            payload.insert("image_width", originalSize.width());
            payload.insert("image_height", originalSize.height());
            payload.insert("preview_width", preview.width());
            payload.insert("preview_height", preview.height());
            respond(m_inferenceClient, m_inferenceRequestId, m_inferenceCommand, payload);
        } else {
            respond(m_inferenceClient, m_inferenceRequestId, m_inferenceCommand, {}, false,
                    payload.value("error").toString("inference worker returned no prediction"));
        }
    } else {
        respond(m_inferenceClient, m_inferenceRequestId, m_inferenceCommand, {}, false,
                "inference worker failed or returned malformed output");
    }
    QProcess *finished = m_inference;
    m_inference = nullptr;
    if (finished) finished->deleteLater();
    m_inferenceClient = nullptr; m_inferenceRequestId.clear(); m_inferenceCommand.clear();
    m_inferenceOutput.clear(); m_inferenceImagePath.clear();
}

void TrainingServer::startTraining(QTcpSocket *socket, const QString &requestId, const QString &task, const QJsonObject &payload)
{
    if (m_worker || m_inference) { respond(socket, requestId, task == "focus" ? "FOCUS_TRAIN_START" : "FAU_TRAIN_START", {}, false, "only one model worker may run at a time"); return; }
    const QString dataset = payload.value("dataset").toString();
    const QString dataPath = datasetPath(task, dataset);
    if (!validId(dataset) || !QFileInfo(dataPath + (task == "focus" ? "/manifest.jsonl" : "/annotations.jsonl")).isFile()) {
        respond(socket, requestId, task == "focus" ? "FOCUS_TRAIN_START" : "FAU_TRAIN_START", {}, false, "dataset is missing required labeled data"); return;
    }
    const QString defaultArchitecture = task == "focus" ? "statistical-logistic-regression" : "gradient-centroid";
    const QString architecture = payload.value("architecture").toString(defaultArchitecture);
    const bool pretrained = (task == "focus" && architecture == "mobilenet_v3_small") ||
                            (task == "fau" && architecture == "resnet18_keypoint");
    const bool baseline = (task == "focus" && architecture == "statistical-logistic-regression") ||
                          (task == "fau" && architecture == "gradient-centroid");
    if (!pretrained && !baseline) { respond(socket, requestId, "ERROR", {}, false, "unsupported model architecture for this task"); return; }
    const int epochs = qBound(1, payload.value("epochs").toInt(task == "focus" ? 20 : 30), 300);
    const int batchSize = qBound(1, payload.value("batch_size").toInt(task == "focus" ? 8 : 4), 256);
    const double learningRate = payload.value("learning_rate").toDouble(pretrained ? 0.0001 : 0.05);
    const QString device = payload.value("device").toString("cpu");
    const QString cameraRole = payload.value("role").toString("top");
    const QString modelName = payload.value("model_name").toString(task == "focus" ? "focus_model" : "fau_model");
    if (!std::isfinite(learningRate) || learningRate <= 0.0 || learningRate > (pretrained ? 0.1 : 1.0)) { respond(socket, requestId, "ERROR", {}, false, "learning rate is outside the allowed finite range"); return; }
    if (device != "cpu" && device != "cuda") { respond(socket, requestId, "ERROR", {}, false, "device must be cpu or cuda"); return; }
    if (baseline && device != "cpu") { respond(socket, requestId, "ERROR", {}, false, "offline baseline runs support CPU only"); return; }
    if (task == "fau" && cameraRole != "top" && cameraRole != "side") { respond(socket, requestId, "ERROR", {}, false, "FAU camera role must be top or side"); return; }
    if (!QRegularExpression("^[A-Za-z0-9_-]{1,48}$").match(modelName).hasMatch()) { respond(socket, requestId, "ERROR", {}, false, "model name must use letters, digits, underscore, or hyphen"); return; }
    m_activeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeTask = task;
    m_activeRunDir = m_projectRoot + "/runs/" + m_activeRunId;
    QDir().mkpath(m_activeRunDir);
    m_jobClient = socket;
    m_cancelRequested = false;
    m_workerTerminalEvent.clear();
    m_workerTerminalPayload = {};
    QFile::remove(m_activeRunDir + "/cancel");
    const QString extension = pretrained ? ".pt" : ".json";
    const QString checkpoint = m_projectRoot + "/models/" + task + "/" + modelName + "_" + m_activeRunId.left(8) + extension;
    QJsonObject config{{"run_id", m_activeRunId}, {"task", task}, {"dataset", dataset}, {"dataset_version", 1},
                       {"architecture", architecture}, {"pretrained", pretrained}, {"model_name", modelName},
                       {"camera_role", cameraRole}, {"epochs", epochs}, {"batch_size", batchSize},
                       {"learning_rate", learningRate}, {"device", device},
                       {"started_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    QFile configFile(m_activeRunDir + "/config.json"); configFile.open(QIODevice::WriteOnly); configFile.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    auto *process = new QProcess(this);
    m_worker = process;
    process->setProgram("python3");
    QString workerPath = m_workerScript;
    QStringList arguments;
    if (pretrained) {
        const QString modelType = task == "focus" ? "focus" : "fau";
        workerPath = QDir(QFileInfo(m_workerScript).absolutePath()).filePath("../" + modelType + "/train.py");
        arguments = {workerPath, "--dataset", dataPath, "--run-id", m_activeRunId,
                     "--epochs", QString::number(epochs), "--batch-size", QString::number(batchSize),
                     "--learning-rate", QString::number(learningRate, 'g', 8), "--device", device,
                     "--checkpoint", checkpoint, "--result", m_activeRunDir + "/result.json",
                     "--cancel-file", m_activeRunDir + "/cancel"};
        if (task == "fau") arguments << "--role" << cameraRole;
    } else {
        arguments = {workerPath, "--task", task, "--dataset", dataPath,
                     "--run-id", m_activeRunId, "--epochs", QString::number(epochs),
                     "--learning-rate", QString::number(learningRate, 'g', 8), "--checkpoint", checkpoint,
                     "--result", m_activeRunDir + "/result.json", "--cancel-file", m_activeRunDir + "/cancel"};
    }
    process->setArguments(arguments);
    connect(process, &QProcess::readyReadStandardOutput, this, &TrainingServer::readWorkerOutput);
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        const QString text = QString::fromUtf8(process->readAllStandardError()).trimmed();
        if (!text.isEmpty()) emitEvent(m_jobClient, "SERVER_LOG", {{"level", "warning"}, {"message", text}});
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &TrainingServer::finishWorker);
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finishWorker(-1, QProcess::CrashExit);
    });
    process->start();
    respond(socket, requestId, task == "focus" ? "FOCUS_TRAIN_START" : "FAU_TRAIN_START",
            {{"run_id", m_activeRunId}, {"state", "PREPARING"}, {"checkpoint", checkpoint},
             {"architecture", architecture}, {"pretrained", pretrained}});
    emitEvent(socket, "TRAINING_STARTED", {{"run_id", m_activeRunId}, {"task", task},
                {"architecture", architecture}, {"pretrained", pretrained}, {"state", "PREPARING"}});
}

void TrainingServer::readWorkerOutput()
{
    if (!m_worker) return;
    m_workerOutput += QString::fromUtf8(m_worker->readAllStandardOutput());
    while (true) {
        const int newline = m_workerOutput.indexOf('\n');
        if (newline < 0) break;
        const QByteArray line = m_workerOutput.left(newline).toUtf8().trimmed();
        m_workerOutput.remove(0, newline + 1);
        const QJsonObject workerMessage = QJsonDocument::fromJson(line).object();
        const QString event = workerMessage.value("event").toString();
        if (!event.isEmpty()) {
            if (event == "TRAINING_COMPLETED" || event == "TRAINING_FAILED" || event == "TRAINING_CANCELLED") {
                m_workerTerminalEvent = event;
                m_workerTerminalPayload = workerMessage;
            } else {
                emitEvent(m_jobClient, event, workerMessage);
            }
        }
    }
}

void TrainingServer::finishWorker(int exitCode, QProcess::ExitStatus exitStatus)
{
    readWorkerOutput();
    if (!m_workerOutput.trimmed().isEmpty()) {
        const QJsonObject message = QJsonDocument::fromJson(m_workerOutput.toUtf8()).object();
        if (!message.isEmpty()) {
            const QString event = message.value("event").toString();
            if (event == "TRAINING_COMPLETED" || event == "TRAINING_FAILED" || event == "TRAINING_CANCELLED") {
                m_workerTerminalEvent = event;
                m_workerTerminalPayload = message;
            } else if (!event.isEmpty()) {
                emitEvent(m_jobClient, event, message);
            }
        }
    }
    if (m_workerTerminalEvent.isEmpty() && (exitStatus != QProcess::NormalExit || (exitCode != 0 && exitCode != 2))) {
        m_workerTerminalEvent = "TRAINING_FAILED";
        m_workerTerminalPayload = {{"run_id", m_activeRunId}, {"error", "worker exited with code " + QString::number(exitCode)}};
    } else if (m_workerTerminalEvent.isEmpty() && (exitCode == 2 || m_cancelRequested)) {
        m_workerTerminalEvent = "TRAINING_CANCELLED";
        m_workerTerminalPayload = {{"run_id", m_activeRunId}, {"task", m_activeTask}};
    }
    const QString resultPath = m_activeRunDir + "/result.json";
    if (!QFileInfo::exists(resultPath) && !m_activeRunId.isEmpty()) {
        QString state = m_workerTerminalEvent == "TRAINING_CANCELLED" ? "CANCELLED" : "FAILED";
        QJsonObject result{{"run_id", m_activeRunId}, {"task", m_activeTask}, {"status", state},
                           {"worker_event", m_workerTerminalEvent},
                           {"error", m_workerTerminalPayload.value("error").toString()},
                           {"finished_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        QFile file(resultPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    }
    QProcess *finished = m_worker;
    m_worker = nullptr;
    if (finished) finished->deleteLater();
    if (!m_workerTerminalEvent.isEmpty()) emitEvent(m_jobClient, m_workerTerminalEvent, m_workerTerminalPayload);
    m_activeRunId.clear(); m_activeTask.clear(); m_activeRunDir.clear(); m_workerOutput.clear(); m_workerTerminalEvent.clear(); m_workerTerminalPayload = {}; m_jobClient = nullptr; m_cancelRequested = false;
}
