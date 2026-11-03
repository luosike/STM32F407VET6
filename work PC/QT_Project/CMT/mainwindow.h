#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QStringList>
#include "serialportcombobox.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    /* 勾选/取消"打开串口" —— 函数名符合 on_<控件名>_<信号名>，setupUi 会自动连接 */
    void on_checkBox_switchusart_toggled(bool checked);

    /* 串口收到数据 */
    void onReadyRead();

    /* 串口异常（典型：USB 被拔掉） */
    void onSerialError(QSerialPort::SerialPortError error);

    void on_pushButton_clear_clicked();

    /* 选择命令文件（.txt） */
    void on_toolButton_changefile_clicked();

    /* 写入数据：读文件 → 逐行发送 */
    void on_pushButton_write_clicked();

private:
    bool openSerialPort();       // 真正打开，成功返回 true
    void closeSerialPort();      // 关闭并释放
    void appendText(const QString &text, bool isRx);   // 往接收区追加一行
    bool sendTextLine(const QString &text, bool labelAsBps = false);   // 文本命令（自动加 \r\n）
    bool sendBinaryFrame(const QByteArray &frame);                     // 二进制帧（原样发送）
    bool writeRaw(const QByteArray &bytes);                            // 底层写 + 等待发送完成

    /* 解析并校验命令文件；成功时 *outCmds = [波特率, REQ报文1, REQ报文2, ...] */
    bool loadCommandFile(const QString &path, QStringList *outCmds, QString *err);

    Ui::MainWindow *ui;
    SerialPortComboBox *portBox = nullptr;   // 指向被"提升"过的下拉框
    QSerialPort *serial = nullptr;           // 串口对象

    QStringList m_cmds;                      // 已解析好的待发送内容
    QString     m_cmdPath;                   // 当前命令文件的完整路径
};

#endif // MAINWINDOW_H
