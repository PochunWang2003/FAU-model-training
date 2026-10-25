#ifndef FAU_LOSS_CHART_WIDGET_H
#define FAU_LOSS_CHART_WIDGET_H

#include <QWidget>
#include <QVector>

class LossChartWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit LossChartWidget(QWidget *parent = nullptr);
    void addPoint(double training, double validation);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_training;
    QVector<double> m_validation;
};

#endif
