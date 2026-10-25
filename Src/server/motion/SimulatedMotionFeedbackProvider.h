#ifndef FAU_SIMULATED_MOTION_FEEDBACK_PROVIDER_H
#define FAU_SIMULATED_MOTION_FEEDBACK_PROVIDER_H

#include "IMotionFeedbackProvider.h"

class SimulatedMotionFeedbackProvider final : public IMotionFeedbackProvider
{
public:
    MotionFeedbackSnapshot ReadSnapshot() const override;
    double Position(const QString &axis) const override;
    void SetSimulatedPosition(const QString &axis, double value);

private:
    std::array<double, 6> m_pose{};
};

#endif
