#include "SimulatedMotionBackend.h"

QString SimulatedMotionBackend::BackendId() const { return "simulated"; }
QString SimulatedMotionBackend::Availability() const { return "SIMULATED"; }
MotionBackendResult SimulatedMotionBackend::Initialize()
{
    return {true, "SIMULATED", "simulated backend initialized; no hardware was accessed"};
}
MotionBackendResult SimulatedMotionBackend::Shutdown()
{
    return {true, "SIMULATED", "simulated backend stopped; no hardware was accessed"};
}
QString SimulatedMotionBackend::ReadMachineState() const { return "SIMULATED_READY"; }
QString SimulatedMotionBackend::ReadMotionStatus() const { return "SIMULATED_IDLE"; }
double SimulatedMotionBackend::Position(const QString &axis) const { return m_feedback.Position(axis); }
MotionFeedbackSnapshot SimulatedMotionBackend::ReadSnapshot() const { return m_feedback.ReadSnapshot(); }
MotionBackendResult SimulatedMotionBackend::ServoOn()
{
    return {false, "SIMULATED", "servo state is not simulated as an operational drive"};
}
MotionBackendResult SimulatedMotionBackend::ServoOff()
{
    return {false, "SIMULATED", "servo state is not simulated as an operational drive"};
}
MotionBackendResult SimulatedMotionBackend::ResetFault()
{
    return {false, "SIMULATED", "drive faults are not simulated"};
}
MotionBackendResult SimulatedMotionBackend::Home(const QJsonObject &)
{
    return {false, "SIMULATED", "homing is disabled in simulation"};
}
MotionBackendResult SimulatedMotionBackend::MoveL(const QJsonObject &)
{
    return {false, "SIMULATED", "MoveL is unavailable; use explicit validated simulation only"};
}
MotionBackendResult SimulatedMotionBackend::CancelStop()
{
    return {true, "SIMULATED", "no simulated motion is active"};
}
QJsonObject SimulatedMotionBackend::ReadMotionConfiguration() const { return {}; }
MotionBackendResult SimulatedMotionBackend::WriteMotionConfiguration(const QJsonObject &)
{
    return {false, "SIMULATED", "only server-validated simulation limits are supported"};
}
void SimulatedMotionBackend::SetSimulatedPosition(const QString &axis, double value)
{
    m_feedback.SetSimulatedPosition(axis, value);
}
