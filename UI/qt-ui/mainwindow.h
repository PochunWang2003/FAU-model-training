#ifndef FAU_MAIN_WINDOW_H
#define FAU_MAIN_WINDOW_H

#include <QMainWindow>
#include <QJsonObject>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void connectServer();
    void readServer();
    void sendCommand(const QString &command, const QJsonObject &payload = {});
    void handleMessage(const QJsonObject &message);
    void appendLog(const QString &text);
    QJsonObject motionLimitsFromTable() const;
    void applyMotionLimits(const QJsonObject &limits);

private:
    Ui::MainWindow *ui;
    QTcpSocket m_socket;
    QByteArray m_buffer;
    quint64 m_requestCounter = 0;
    QString m_focusDataset = "focus_demo";
    QString m_fauDataset = "fau_demo";
};

#endif
