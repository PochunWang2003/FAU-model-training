#ifndef FAU_SYNTHETIC_CAMERA_SERVICE_H
#define FAU_SYNTHETIC_CAMERA_SERVICE_H

#include "ICameraAcquisitionService.h"

class SyntheticCameraService final : public ICameraAcquisitionService
{
public:
    QStringList Roles() const override;
    CameraServiceStatus Status(const QString &role) const override;
    QImage AcquireSnapshot(const QString &role, int seed, bool focusDemo) override;
};

#endif
