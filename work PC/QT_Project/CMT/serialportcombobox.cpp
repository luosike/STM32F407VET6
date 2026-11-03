// serialportcombobox.cpp
#include "serialportcombobox.h"

#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QTimer>
#include <QAbstractItemView>

SerialPortComboBox::SerialPortComboBox(QWidget *parent) : QComboBox(parent)
{
    setMinimumContentsLength(24);
    setSizeAdjustPolicy(QComboBox::AdjustToContents);

    refreshPorts();                     // 初始化先扫一次

    /* ★ 热插拔：定时刷新。插上 USB 后 1.5 秒内自动出现在列表里 */
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SerialPortComboBox::onRefreshTimer);
    m_timer->start(1500);
}

void SerialPortComboBox::showPopup()
{
    refreshPorts();                     // 弹出前再刷一次，保证是最新的
    QComboBox::showPopup();
}

void SerialPortComboBox::onRefreshTimer()
{
    refreshPorts();
}

void SerialPortComboBox::refreshPorts()
{
    /* ---- 三种情况跳过刷新，避免打架 ---- */

    /* 1) 打开串口期间，外部会 setEnabled(false)，这时不要动列表 */
    if (!isEnabled())
        return;

    /* 2) 下拉列表正在显示时重建会打断用户点击 */
    if (view() && view()->isVisible())
        return;

    /* 3) 枚举当前可用串口 */
    const auto ports = QSerialPortInfo::availablePorts();

    QStringList names;
    names.reserve(ports.size());
    for (const QSerialPortInfo &p : ports)
        names << p.portName();

    /* 4) 端口列表没变化 → 不重建（省开销、不出闪烁） */
    if (!m_firstRefresh && names == m_lastNames && count() > 0)
        return;

    m_firstRefresh = false;
    m_lastNames = names;

    /* 5) 记住当前选中的端口名，重建后尽量恢复 */
    const QString cur = currentData().toString();

    QSignalBlocker blocker(this);       // 避免 clear/addItem 触发 currentIndexChanged
    clear();

    if (names.isEmpty())
    {
        /* ★ 这里不再 setEnabled(false)！
         * 否则程序先启动、后插 USB 时，下拉框会一直是灰的，点都点不开 */
        addItem("（未检测到串口）", QString());
        setCurrentIndex(0);
        return;
    }

    for (const QSerialPortInfo &p : ports)
    {
        const QString desc = p.description().isEmpty() ? "未知设备" : p.description();
        const QString text = QString("%1：%2").arg(p.portName(), desc);

        addItem(text, p.portName());                        // userData = "COM3"
        setItemData(count() - 1, p.systemLocation(), Qt::ToolTipRole);
    }

    /* 6) 恢复原来的选择；找不到就选第 0 个 */
    const int idx = findData(cur);
    setCurrentIndex(idx >= 0 ? idx : 0);
}
