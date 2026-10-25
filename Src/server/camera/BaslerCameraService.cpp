#include "BaslerCameraService.h"

QStringList BaslerCameraService::Roles() const { return {"top", "side"}; }

CameraServiceStatus BaslerCameraService::Status(const QString &role) const
{
    if (role != "top" && role != "side") return {"basler", "UNAVAILABLE", "unknown camera role"};
    return {"basler", "UNAVAILABLE", "Basler pylon integration is not configured"};
}

QImage BaslerCameraService::AcquireSnapshot(const QString &, int, bool)
{
    return {};
}
