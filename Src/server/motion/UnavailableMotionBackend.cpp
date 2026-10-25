#include "UnavailableMotionBackend.h"

#include <limits>

UnavailableMotionBackend::UnavailableMotionBackend(const QString &backendId)
    : m_backendId(backendId)
{
}

QString UnavailableMotionBackend::BackendId() const { return m_backendId; }
QString UnavailableMotionBackend::Availability() const { return "UNAVAILABLE"; }
MotionBackendResult UnavailableMotionBackend::UnavailableResult(const QString &operation) const
{
    return {false, "UNAVAILABLE", m_backendId + " backend is unavailable; " + operation + " was not sent"};
}
MotionBackendResult UnavailableMotionBackend::Initialize() { return UnavailableResult("initialize"); }
MotionBackendResult UnavailableMotionBackend::Shutdown() { return {true, "UNAVAILABLE", "no physical backend is active"}; }
QString UnavailableMotionBackend::ReadMachineState() const { return "UNAVAILABLE"; }
QString UnavailableMotionBackend::ReadMotionStatus() const { return "UNAVAILABLE"; }
double UnavailableMotionBackend::Position(const QString &) const { return std::numeric_limits<double>::quiet_NaN(); }
MotionFeedbackSnapshot UnavailableMotionBackend::ReadSnapshot() const { return {}; }
MotionBackendResult UnavailableMotionBackend::ServoOn() { return UnavailableResult("Servo ON"); }
MotionBackendResult UnavailableMotionBackend::ServoOff() { return UnavailableResult("Servo OFF"); }
MotionBackendResult UnavailableMotionBackend::ResetFault() { return UnavailableResult("fault reset"); }
MotionBackendResult UnavailableMotionBackend::Home(const QJsonObject &) { return UnavailableResult("homing"); }
MotionBackendResult UnavailableMotionBackend::MoveL(const QJsonObject &) { return UnavailableResult("MoveL"); }
MotionBackendResult UnavailableMotionBackend::CancelStop() { return UnavailableResult("cancel/stop"); }
QJsonObject UnavailableMotionBackend::ReadMotionConfiguration() const { return {}; }
MotionBackendResult UnavailableMotionBackend::WriteMotionConfiguration(const QJsonObject &)
{
    return UnavailableResult("configuration write");
}
