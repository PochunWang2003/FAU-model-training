#ifndef FAU_SIMULATED_MOTION_BACKEND_H
#define FAU_SIMULATED_MOTION_BACKEND_H

#include "IMotionBackend.h"
#include "SimulatedMotionFeedbackProvider.h"

class SimulatedMotionBackend final : public IMotionBackend
{
public:
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
    void SetSimulatedPosition(const QString &axis, double value);

private:
    SimulatedMotionFeedbackProvider m_feedback;
};

#endif
