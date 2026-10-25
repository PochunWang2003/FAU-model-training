#include "SyntheticCameraService.h"

#include <QtGlobal>

QStringList SyntheticCameraService::Roles() const { return {"top", "side"}; }

CameraServiceStatus SyntheticCameraService::Status(const QString &role) const
{
    if (role != "top" && role != "side") return {"synthetic", "UNAVAILABLE", "unknown camera role"};
    return {"synthetic", "SIMULATED", "synthetic frame source; no physical camera was opened"};
}

QImage SyntheticCameraService::AcquireSnapshot(const QString &role, int seed, bool focusDemo)
{
    if (role != "top" && role != "side") return {};
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
    return image.copy();
}
