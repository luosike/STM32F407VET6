#ifndef SERIALPORTCOMBOBOX_H
#define SERIALPORTCOMBOBOX_H

// serialportcombobox.h
#pragma once
#include <QComboBox>
#include <QStringList>

class QTimer;

class SerialPortComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit SerialPortComboBox(QWidget *parent = nullptr);

    /* 手动刷新（也可以外部定时调用） */
    void refreshPorts();

protected:
    /* 点击下拉时先刷新，再弹出 */
    void showPopup() override;

private slots:
    void onRefreshTimer();       // 定时器槽：热插拔自动刷新

private:
    QTimer *m_timer = nullptr;

    /* 上一次的端口名列表，用来判断"有没有变化"，没变化就不重建，省得闪 */
    QStringList m_lastNames;
    bool m_firstRefresh = true;
};

#endif // SERIALPORTCOMBOBOX_H
