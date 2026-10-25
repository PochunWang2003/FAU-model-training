#ifndef FAU_TRAINING_SERVER_H
#define FAU_TRAINING_SERVER_H

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "motion/IMotionFeedbackProvider.h"
#include "motion/SimulatedMotionFeedbackProvider.h"

#include <memory>

class TrainingServer final : public QObject
{
    Q_OBJECT
public:
    explicit TrainingServer(const QString &projectRoot, QObject *parent = nullptr);
    bool listen(const QHostAddress &address, quint16 port);

private slots:
    void acceptConnections();
    void readClient();
    void removeClient();
    void sendPreviewFrames();
    void readWorkerOutput();
    void finishWorker(int exitCode, QProcess::ExitStatus status);
    void readInferenceOutput();
    void finishInference(int exitCode, QProcess::ExitStatus status);

private:
    void processMessage(QTcpSocket *socket, const QJsonObject &message);
    void respond(QTcpSocket *socket, const QString &requestId, const QString &event,
                 const QJsonObject &payload = {}, bool ok = true, const QString &error = {});
    void emitEvent(QTcpSocket *socket, const QString &event, const QJsonObject &payload = {});
    bool validId(const QString &value) const;
    QString datasetPath(const QString &kind, const QString &id) const;
    QImage syntheticFrame(const QString &role, int seed, bool focusDemo) const;
    bool writePgm(const QImage &image, const QString &path) const;
    bool createDataset(const QString &kind, const QString &id, int count, QString *error);
    void startTraining(QTcpSocket *socket, const QString &requestId, const QString &task,
                       const QJsonObject &payload);
    void startInference(QTcpSocket *socket, const QString &requestId, const QString &task,
                        const QJsonObject &payload);
    QJsonObject status() const;
    QJsonObject defaultLimits() const;
    QString validateLimits(const QJsonObject &limits) const;
    QString validateMotion(const QJsonObject &request) const;
    QJsonObject poseMap() const;

    QString m_projectRoot;
    QString m_workerScript;
    QTcpServer m_listener;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QTcpSocket *, QSet<QString>> m_seenRequests;
    QHash<QTcpSocket *, QSet<QString>> m_streamingRoles;
    QTimer m_previewTimer;
    QPointer<QProcess> m_worker;
    QPointer<QProcess> m_inference;
    QPointer<QTcpSocket> m_jobClient;
    QPointer<QTcpSocket> m_inferenceClient;
    QString m_activeRunId;
    QString m_activeTask;
    QString m_activeRunDir;
    QString m_workerOutput;
    QString m_workerTerminalEvent;
    QJsonObject m_workerTerminalPayload;
    QString m_inferenceRequestId;
    QString m_inferenceCommand;
    QString m_inferenceOutput;
    QString m_inferenceImagePath;
    bool m_cancelRequested = false;
    QJsonObject m_limits;
    std::unique_ptr<IMotionFeedbackProvider> m_motionFeedback;
    SimulatedMotionFeedbackProvider *m_simulatedMotionFeedback = nullptr;
};

#endif
