#include "SimulatedMotionFeedbackProvider.h"

#include <QJsonArray>
#include <QStringList>

namespace {
const QStringList kAxes{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                        QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")};

QString stateName(FeedbackState state)
{
    switch (state) {
    case FeedbackState::Simulated: return QStringLiteral("SIMULATED");
    case FeedbackState::Real: return QStringLiteral("REAL");
    case FeedbackState::Unavailable: return QStringLiteral("UNAVAILABLE");
    }
    return QStringLiteral("UNAVAILABLE");
}
}

QJsonObject MotionFeedbackSnapshot::ToJson() const
{
    QJsonArray poseValues;
    QJsonArray countValues;
    for (size_t index = 0; index < pose.size(); ++index) {
        poseValues.append(pose[index]);
        countValues.append(rawCountsAvailable ? QJsonValue(static_cast<double>(rawCounts[index])) : QJsonValue(QJsonValue::Null));
    }
    return {{QStringLiteral("pose_source"), stateName(poseState)},
            {QStringLiteral("pose_units"), QJsonArray{QStringLiteral("mm"),QStringLiteral("mm"),QStringLiteral("mm"),QStringLiteral("deg"),QStringLiteral("deg"),QStringLiteral("deg")}},
            {QStringLiteral("pose"), poseValues},
            {QStringLiteral("raw_count_source"), stateName(countState)},
            {QStringLiteral("raw_counts"), countValues},
            {QStringLiteral("raw_counts_available"), rawCountsAvailable}};
}

MotionFeedbackSnapshot SimulatedMotionFeedbackProvider::ReadSnapshot() const
{
    MotionFeedbackSnapshot snapshot;
    snapshot.poseState = FeedbackState::Simulated;
    snapshot.pose = m_pose;
    snapshot.countState = FeedbackState::Unavailable;
    snapshot.rawCountsAvailable = false;
    return snapshot;
}

double SimulatedMotionFeedbackProvider::Position(const QString &axis) const
{
    const qsizetype index = kAxes.indexOf(axis);
    return index < 0 ? 0.0 : m_pose[static_cast<size_t>(index)];
}

void SimulatedMotionFeedbackProvider::SetSimulatedPosition(const QString &axis, double value)
{
    const qsizetype index = kAxes.indexOf(axis);
    if (index >= 0) m_pose[static_cast<size_t>(index)] = value;
}
