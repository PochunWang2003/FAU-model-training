#ifndef FAU_BASLER_CAMERA_SERVICE_H
#define FAU_BASLER_CAMERA_SERVICE_H

#include "ICameraAcquisitionService.h"

class BaslerCameraService final : public ICameraAcquisitionService
{
public:
    QStringList Roles() const override;
    CameraServiceStatus Status(const QString &role) const override;
    QImage AcquireSnapshot(const QString &role, int seed, bool focusDemo) override;
};

#endif
