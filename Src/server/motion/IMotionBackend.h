#ifndef FAU_IMOTION_BACKEND_H
#define FAU_IMOTION_BACKEND_H

#include "IMotionFeedbackProvider.h"

#include <QJsonObject>
#include <QString>

struct MotionBackendResult
{
    bool accepted = false;
    QString availability = "UNAVAILABLE";
    QString message = "motion backend is unavailable";
};

class IMotionBackend : public IMotionFeedbackProvider
{
public:
    ~IMotionBackend() override = default;

    virtual QString BackendId() const = 0;
    virtual QString Availability() const = 0;
    virtual MotionBackendResult Initialize() = 0;
    virtual MotionBackendResult Shutdown() = 0;
    virtual QString ReadMachineState() const = 0;
    virtual QString ReadMotionStatus() const = 0;
    virtual MotionBackendResult ServoOn() = 0;
    virtual MotionBackendResult ServoOff() = 0;
    virtual MotionBackendResult ResetFault() = 0;
    virtual MotionBackendResult Home(const QJsonObject &request) = 0;
    virtual MotionBackendResult MoveL(const QJsonObject &request) = 0;
    virtual MotionBackendResult CancelStop() = 0;
    virtual QJsonObject ReadMotionConfiguration() const = 0;
    virtual MotionBackendResult WriteMotionConfiguration(const QJsonObject &configuration) = 0;
};

#endif
