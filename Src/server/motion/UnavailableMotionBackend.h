#ifndef FAU_UNAVAILABLE_MOTION_BACKEND_H
#define FAU_UNAVAILABLE_MOTION_BACKEND_H

#include "IMotionBackend.h"

class UnavailableMotionBackend : public IMotionBackend
{
public:
    explicit UnavailableMotionBackend(const QString &backendId);

    QString BackendId() const override;
    QString Availability() const override;
    MotionBackendResult Initialize() override;
    MotionBackendResult Shutdown() override;
    QString ReadMachineState() const override;
    QString ReadMotionStatus() const override;
    double Position(const QString &axis) const override;
    MotionFeedbackSnapshot ReadSnapshot() const override;
    MotionBackendResult ServoOn() override;
    MotionBackendResult ServoOff() override;
    MotionBackendResult ResetFault() override;
    MotionBackendResult Home(const QJsonObject &request) override;
    MotionBackendResult MoveL(const QJsonObject &request) override;
    MotionBackendResult CancelStop() override;
    QJsonObject ReadMotionConfiguration() const override;
    MotionBackendResult WriteMotionConfiguration(const QJsonObject &configuration) override;

private:
    MotionBackendResult UnavailableResult(const QString &operation) const;
    QString m_backendId;
};

#endif
