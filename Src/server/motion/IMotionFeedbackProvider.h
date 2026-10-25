#ifndef FAU_MOTION_FEEDBACK_PROVIDER_H
#define FAU_MOTION_FEEDBACK_PROVIDER_H

#include <QJsonObject>
#include <QString>

#include <array>

enum class FeedbackState { Simulated, Unavailable, Real };

struct MotionFeedbackSnapshot
{
    FeedbackState poseState = FeedbackState::Unavailable;
    std::array<double, 6> pose{};
    FeedbackState countState = FeedbackState::Unavailable;
    std::array<qint64, 6> rawCounts{};
    bool rawCountsAvailable = false;

    QJsonObject ToJson() const;
};

class IMotionFeedbackProvider
{
public:
    virtual ~IMotionFeedbackProvider() = default;
    virtual MotionFeedbackSnapshot ReadSnapshot() const = 0;
    virtual double Position(const QString &axis) const = 0;
};

#endif
