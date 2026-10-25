#include "losschartwidget.h"

#include <QPainter>
#include <QPainterPath>

LossChartWidget::LossChartWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(130);
}

void LossChartWidget::addPoint(double training, double validation)
{
    m_training.append(training);
    m_validation.append(validation);
    update();
}

void LossChartWidget::clear()
{
    m_training.clear();
    m_validation.clear();
    update();
}

void LossChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#202833"));
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = rect().adjusted(34, 12, -12, -22);
    painter.setPen(QColor("#708090"));
    painter.drawRect(plot);
    painter.setPen(QColor("#d9e2ec"));
    painter.drawText(5, 16, "Loss");
    if (m_training.isEmpty()) {
        painter.drawText(plot, Qt::AlignCenter, "Training loss and validation loss will appear here");
        return;
    }
    double maximum = 0.001;
    for (double value : m_training) maximum = qMax(maximum, value);
    for (double value : m_validation) maximum = qMax(maximum, value);
    auto drawSeries = [&](const QVector<double> &values, const QColor &color) {
        if (values.size() < 2) return;
        QPainterPath path;
        for (int index = 0; index < values.size(); ++index) {
            const double x = plot.left() + (plot.width() * index / qMax(1, values.size() - 1));
            const double y = plot.bottom() - plot.height() * qBound(0.0, values[index] / maximum, 1.0);
            if (index == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        painter.setPen(QPen(color, 2));
        painter.drawPath(path);
    };
    drawSeries(m_training, QColor("#55c2ff"));
    drawSeries(m_validation, QColor("#ffb454"));
    painter.setPen(QColor("#55c2ff")); painter.drawText(42, height() - 4, "Train");
    painter.setPen(QColor("#ffb454")); painter.drawText(92, height() - 4, "Validation");
}
