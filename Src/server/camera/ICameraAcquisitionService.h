#ifndef FAU_ICAMERA_ACQUISITION_SERVICE_H
#define FAU_ICAMERA_ACQUISITION_SERVICE_H

#include <QImage>
#include <QString>
#include <QStringList>

struct CameraServiceStatus
{
    QString backend = "unavailable";
    QString availability = "UNAVAILABLE";
    QString message = "camera service is unavailable";
};

class ICameraAcquisitionService
{
public:
    virtual ~ICameraAcquisitionService() = default;
    virtual QStringList Roles() const = 0;
    virtual CameraServiceStatus Status(const QString &role) const = 0;
    virtual QImage AcquireSnapshot(const QString &role, int seed, bool focusDemo) = 0;
};

#endif
