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
    QJsonObject status() const;
    QJsonObject defaultLimits() const;
    QString validateLimits(const QJsonObject &limits) const;
    QString validateMotion(const QJsonObject &request) const;

    QString m_projectRoot;
    QString m_workerScript;
    QTcpServer m_listener;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QTcpSocket *, QSet<QString>> m_seenRequests;
    QHash<QTcpSocket *, QSet<QString>> m_streamingRoles;
    QTimer m_previewTimer;
    QPointer<QProcess> m_worker;
    QPointer<QTcpSocket> m_jobClient;
    QString m_activeRunId;
    QString m_activeTask;
    QString m_activeRunDir;
    QString m_workerOutput;
    QString m_workerTerminalEvent;
    bool m_cancelRequested = false;
    QJsonObject m_limits;
    QJsonObject m_positions;
};

#endif
