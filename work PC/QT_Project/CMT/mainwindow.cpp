#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QSerialPortInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QTextCursor>
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

/* ============================================================
 *  文件行解析工具（只在本文件用）
 * ============================================================ */

/**
 * @brief 解析波特率：支持 "500K" / "1M" / "500000" / "500 k"（大小写都行）
 * @return true 成功；*out 是转换后的数字（如 500K → 500000）
 */
static bool parseBaudRate(const QString &raw, quint32 *out)
{
    QString t = raw.trimmed();
    if (t.isEmpty())
        return false;

    /* 兼容 "BPS:500K" / "bps : 500K" 这种带前缀的写法（前缀可有可无） */
    static const QRegularExpression reBpsPrefix(QStringLiteral("^BPS\\s*:\\s*"),
                                                QRegularExpression::CaseInsensitiveOption);
    t.remove(reBpsPrefix);

    if (t.isEmpty())
        return false;

    quint64 mul = 1;
    const QChar last = t.at(t.size() - 1);
    if (last == QLatin1Char('k') || last == QLatin1Char('K'))
    {
        mul = 1000ULL;
        t.chop(1);                      /* 去掉结尾的 k */
    }
    else if (last == QLatin1Char('m') || last == QLatin1Char('M'))
    {
        mul = 1000000ULL;
        t.chop(1);                      /* 去掉结尾的 M */
    }

    t = t.trimmed();                    /* 兼容 "500 K" 这种中间有空格 */
    bool ok = false;
    const quint64 v = t.toULongLong(&ok);
    if (!ok || v == 0ULL)
        return false;

    *out = static_cast<quint32>(v * mul);
    return true;
}

/**
 * @brief 解析一行 CAN 报文
 *        兼容：多空格 / 制表符 / 逗号分隔；"0x" 前缀；单个十六进制位自动补 0（1 → 01）
 * @param outHex 输出：去掉所有空格、每个字节补足 2 位的连续十六进制串
 *               例如 "88   42 26 1 2 3" → "884226010203"
 * @return true 成功；false 时 *err 里是错误原因
 */
static bool parseCanHexLine(const QString &raw, QString *outHex, QString *err)
{    /* 空格、制表符、逗号都当分隔符，连续的多个也一并处理 */
    const QStringList tokens = raw.split(QRegularExpression(QStringLiteral("[\\s,]+")),
                                         Qt::SkipEmptyParts);

    static const QRegularExpression reHex(QStringLiteral("^[0-9A-Fa-f]{1,2}$"));

    QString hex;
    hex.reserve(tokens.size() * 2);

    for (const QString &tk : tokens)
    {
        QString h = tk;
        if (h.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            h.remove(0, 2);             /* 去掉 0x 前缀 */

        if (!reHex.match(h).hasMatch())
        {
            *err = QString("非法十六进制“%1”").arg(tk);
            return false;
        }

        if (h.size() == 1)              /* ★ 写 1 时补成 01 */
            h.prepend(QLatin1Char('0'));

        hex += h.toUpper();
    }

    if (hex.isEmpty())
    {
        *err = "该行没有数据";
        return false;
    }

    *outHex = hex;
    return true;
}

/**
 * @brief 把连续的十六进制串加上空格： "8842260102" → "88 42 26 01 02"
 */
static QString hexToSpaced(const QString &hexNoSpace)
{
    QString out;
    out.reserve(hexNoSpace.size() + hexNoSpace.size() / 2);

    for (int i = 0; i < hexNoSpace.size(); i += 2)
    {
        if (i != 0)
            out += QLatin1Char(' ');
        out += hexNoSpace.mid(i, 2);
    }
    return out;
}

/**
 * @brief 解析一行 "REQ:88 42 26 01 ... , ANS:88 42 26 ..."
 *        只取 REQ: 之后、第一个逗号之前的内容（ANS 部分整个丢掉）
 * @return true 成功；false 时 *err 是原因
 */
static bool parseReqLine(const QString &raw, QString *outHex, QString *err)
{
    static const QRegularExpression reReq(QStringLiteral("REQ\\s*:"),
                                          QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = reReq.match(raw);
    if (!m.hasMatch())
    {
        *err = "缺少 REQ: 标记";
        return false;
    }

    QString body = raw.mid(m.capturedEnd());          /* REQ: 之后的内容 */

    const int comma = body.indexOf(QLatin1Char(',')); /* 只取到第一个逗号 */
    if (comma >= 0)
        body.truncate(comma);

    return parseCanHexLine(body, outHex, err);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /* 提升后的控件本身就是 SerialPortComboBox*，直接赋值即可 */
    portBox = ui->comboBox_changeusart;

    /* 下拉框每次弹出都会自动重新扫描串口（SerialPortComboBox::showPopup 里做的） */

    ui->checkBox_switchusart->setChecked(false);
    ui->checkBox_switchusart->setText("打开串口");
}

MainWindow::~MainWindow()
{
    closeSerialPort();          /* 关窗口时确保串口被释放 */
    delete ui;
}

/* ============================================================
 *  勾选框：勾上 = 打开串口，取消 = 关闭串口
 * ============================================================ */
void MainWindow::on_checkBox_switchusart_toggled(bool checked)
{
    if (checked)
    {
        /* 防止重复打开 */
        if (serial && serial->isOpen())
            return;

        // 每次重新打开串口 先清空界面
        ui->textEdit->clear();
        if (!openSerialPort())
        {
            /* 打开失败：把勾去掉（会再次进入本函数走关闭分支，把界面恢复） */
            ui->checkBox_switchusart->setChecked(false);
            return;
        }

        ui->checkBox_switchusart->setText("关闭串口");
        ui->comboBox_changeusart->setEnabled(false);   /* 通信期间禁止切换串口 */
    }
    else
    {
        closeSerialPort();

        ui->checkBox_switchusart->setText("打开串口");
        ui->comboBox_changeusart->setEnabled(true);
    }
}

/* ============================================================
 *  打开串口
 * ============================================================ */
bool MainWindow::openSerialPort()
{
    /* 1) 取当前选中的端口名（userData 里存的是 "COM3"） */
    const QString portName = portBox->currentData().toString();
    if (portName.isEmpty())
    {
        QMessageBox::warning(this, "提示", "没有可用的串口，请插入设备后再试。");
        return false;
    }

    /* 2) 创建对象并连接信号 */
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead,    this, &MainWindow::onReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);

    /* 3) 配置参数
     *    注意：如果连的是 STM32 的 USB CDC 虚拟串口，波特率其实无意义（USB 全速固定带宽），
     *    但连 USB转TTL 时就要按设备要求设，比如 9600 / 115200 */
    serial->setPortName(portName);
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    /* 4) 打开 */
    if (!serial->open(QIODevice::ReadWrite))
    {
        QMessageBox::critical(this, "打开失败",
            QString("无法打开 %1\n\n原因：%2")
                .arg(portName, serial->errorString()));

        delete serial;
        serial = nullptr;
        return false;
    }

    appendText(QString("=== 已打开 %1（%2）===")
                   .arg(portName, portBox->currentText()), false);
    return true;
}

/* ============================================================
 *  关闭串口
 * ============================================================ */
void MainWindow::closeSerialPort()
{
    if (serial == nullptr)
        return;

    if (serial->isOpen())
        serial->close();

    serial->deleteLater();      /* 交给事件循环安全释放 */
    serial = nullptr;

    appendText("=== 串口已关闭 ===", false);
}

/* ============================================================
 *  收到数据：按字节显示成空格分隔的十六进制
 * ============================================================ */
void MainWindow::onReadyRead()
{
    if (serial == nullptr)
        return;

    const QByteArray data = serial->readAll();
    if (data.isEmpty())
        return;

    /* 全是 0x00 的帧跳过（设备空闲时可能一直发空包，避免刷屏）。
     * 如果你确实想看这些空数据，把下面这段判断删掉即可 */
    bool allZero = true;
    for (char c : data)
    {
        if (c != 0)
        {
            allZero = false;
            break;
        }
    }
    if (allZero)
        return;

    /* 88 42 26 01 00 01 02 03 ... */
    const QString hex = QString::fromLatin1(data.toHex(' ')).toUpper();
    appendText(hex, true);
}

/* ============================================================
 *  串口异常（最常见：设备被拔掉 → ResourceError）
 * ============================================================ */
void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    if (error == QSerialPort::ResourceError)
    {
        QMessageBox::warning(this, "串口异常", "设备已断开，串口将被关闭。");

        /* 取消勾选 → 自动走关闭分支，把界面恢复 */
        ui->checkBox_switchusart->setChecked(false);
    }
}

/* ============================================================
 *  加载并校验命令文件（选文件时就调用，不等到发送才判断）
 *  文件格式：
 *      第 1 条有效行：波特率，支持 500K / 1M / 500000
 *      之后每行：REQ:88 42 26 01 ... , ANS:...
 *                ★只取 REQ: 之后、第一个逗号之前的内容，ANS 部分全部忽略
 *  兼容：多空格 / 制表符 / 逗号分隔；"0x" 前缀；单个 hex 位自动补 0（1 → 01）
 * ============================================================ */
bool MainWindow::loadCommandFile(const QString &path, QStringList *outCmds, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        *err = QString("打不开文件：%1").arg(f.errorString());
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QString text = QString::fromUtf8(raw);
    if (text.startsWith(QChar(0xFEFF)))                 /* 去掉 UTF-8 BOM */
        text.remove(0, 1);

    /* 按 '\n' 拆分并去掉行尾 '\r'。
     * ★ 不能用 SkipEmptyParts：空行必须保留占位，否则报错行号会和编辑器对不上 */
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &one : lines)
    {
        if (one.endsWith(QLatin1Char('\r')))
            one.chop(1);
    }

    QStringList cmds;
    bool baudDone = false;
    int  lineNo   = 0;

    for (const QString &rawLine : lines)
    {
        ++lineNo;
        const QString line = rawLine.trimmed();

        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1String("//")))
            continue;                                   /* 注释行，跳过 */

        if (!baudDone)
        {
            /* ---------- 第 1 条有效行：波特率 ---------- */
            quint32 baud = 0;
            if (!parseBaudRate(line, &baud))
            {
                *err = QString("第 %1 行：波特率格式不对\n\n"
                               "  原文：%2\n\n"
                               "BPS: 后面应直接跟数字（可带 K/M 后缀）\n"
                               "正确写法：BPS:500K、BPS:1M、BPS:500000\n"
                               "（BPS: 前缀也可以省略）").arg(lineNo).arg(line);
                return false;
            }
            cmds << QString::number(baud);              /* 500K → 500000 */
            baudDone = true;
            continue;
        }

        /* ---------- 之后的行：只取 REQ 内容 ---------- */
        QString hex, e;
        if (!parseReqLine(line, &hex, &e))
        {
            *err = QString("第 %1 行：%2\n\n原始内容：%3").arg(lineNo).arg(e, line);
            return false;
        }
        cmds << hex;
    }

    if (!baudDone)
    {
        *err = "文件里没有找到波特率行（第 1 行应类似 500K 或 500000）";
        return false;
    }
    if (cmds.size() < 2)
    {
        *err = "文件里没有 REQ 报文行（应类似 REQ:88 42 26 01 ...）";
        return false;
    }

    *outCmds = cmds;
    return true;
}

/* ============================================================
 *  写入数据：把已解析好的内容逐行发出去
 * ============================================================ */
void MainWindow::on_pushButton_write_clicked()
{
    if (serial == nullptr || !serial->isOpen())
    {
        QMessageBox::warning(this, "提示", "请先打开串口。");
        return;
    }

    /* 以输入框里显示的路径为准（选文件时填进去的）；
     * 每次发送前重新解析一次，这样你中途改了 txt 内容也能生效 */
    QString path = ui->lineEdit_filename->text().trimmed();
    if (path.isEmpty())
        path = m_cmdPath;
    if (path.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先选择命令文件（点右侧“...”按钮）。");
        return;
    }

    QStringList cmds;
    QString err;
    if (!loadCommandFile(path, &cmds, &err))
    {
        QMessageBox::critical(this, "文件格式错误",
            QString("%1\n\n文件：%2").arg(err, QDir::toNativeSeparators(path)));
        return;
    }

    /* cmds[0] = 波特率（纯数字，发文本），cmds[1..] = 每条 REQ 报文（发二进制） */
    for (int i = 0; i < cmds.size(); ++i)
    {
        if (i == 0)
        {
            /* 第 1 行：波特率，仍然发文本 "500000\r\n"（单片机用 atoi 之类解析） */
            if (!sendTextLine(cmds.at(0), true))
                return;
        }
        else
        {
            /* 之后：十六进制串 → 原始字节，单片机收到的就是 0x88 0x42 0x26 ... */
            const QByteArray frame = QByteArray::fromHex(cmds.at(i).toLatin1());
            if (!sendBinaryFrame(frame))
                return;
        }
    }

    appendText(QString("=== 发送完成：报文 %1 条 ===").arg(cmds.size() - 1), false);
}

/* ============================================================
 *  底层发送：写 + 等待真正发出
 * ============================================================ */
bool MainWindow::writeRaw(const QByteArray &bytes)
{
    if (serial == nullptr || !serial->isOpen())
        return false;

    const qint64 n = serial->write(bytes);
    if (n != bytes.size())
    {
        QMessageBox::critical(this, "发送失败",
            QString("只写入了 %1 / %2 字节").arg(n).arg(bytes.size()));
        return false;
    }

    /* 等数据真正发出去，避免内部发送缓冲越积越多 */
    if (!serial->waitForBytesWritten(1000))
    {
        QMessageBox::critical(this, "发送超时", "等待串口发送完成超时。");
        return false;
    }
    return true;
}

/* ============================================================
 *  发送一条文本命令（自动补 \r\n）
 *  目前只有第 1 行波特率用它：发 "500000\r\n"
 * ============================================================ */
bool MainWindow::sendTextLine(const QString &text, bool labelAsBps)
{
    QByteArray bytes = text.toLatin1();
    bytes.append("\r\n");

    if (!writeRaw(bytes))
        return false;

    appendText(labelAsBps ? QString("BPS: %1").arg(text) : text, false);
    return true;
}

/* ============================================================
 *  发送一帧二进制数据（★ 单片机收到的就是这些字节，可直接转发给 CAN）
 *  参数是已经从十六进制解析好的原始字节，例如 88 42 26 01 ...
 *  注意：帧之间【不加】任何分隔符，单片机需按固定帧长切分
 * ============================================================ */
bool MainWindow::sendBinaryFrame(const QByteArray &frame)
{
    if (frame.isEmpty())
        return false;

    if (!writeRaw(frame))
        return false;

    appendText(QString("REQ: %1")
                   .arg(QString::fromLatin1(frame.toHex(' ')).toUpper()),
               false);
    return true;
}

/* ============================================================
 *  往接收区追加内容（带时间戳）
 * ============================================================ */
void MainWindow::appendText(const QString &text, bool isRx)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(QString("[%1] %2%3\r\n")
                                  .arg(ts, isRx ? "ANS: " : "", text));
    ui->textEdit->moveCursor(QTextCursor::End);
}

/* ============================================================
 *  选择命令文件（只挑 .txt）
 * ============================================================ */
void MainWindow::on_toolButton_changefile_clicked()
{
    /* 默认打开哪个目录？
     *   QCoreApplication::applicationDirPath() → exe 所在目录（推荐，文件放 exe 旁边最方便）
     *   QDir::currentPath()                    → 进程工作目录（在 Qt Creator 里运行是 build 目录，容易找不到文件）
     */
    QString startDir = QCoreApplication::applicationDirPath();

    /* 如果输入框里已经有路径，就优先从那个文件所在目录开始 */
    const QString oldPath = ui->lineEdit_filename->text().trimmed();
    if (!oldPath.isEmpty())
    {
        const QFileInfo fi(oldPath);
        if (fi.absoluteDir().exists())
            startDir = fi.absolutePath();
    }

    const QString file = QFileDialog::getOpenFileName(
        this,
        "选择命令文件",
        startDir,
        "文本文件 (*.txt);;所有文件 (*.*)");

    if (file.isEmpty())         /* 用户点了取消 */
        return;

    /* ★ 选完文件立刻校验格式，不合格就不接受这个文件 */
    QStringList cmds;
    QString err;
    if (!loadCommandFile(file, &cmds, &err))
    {
        m_cmds.clear();
        m_cmdPath.clear();
        QMessageBox::critical(this, "文件格式错误",
            QString("%1\n\n文件：%2\n\n（这个文件不会被使用，请修正后重新选择）")
                .arg(err, QDir::toNativeSeparators(file)));
        return;
    }

    m_cmds    = cmds;
    m_cmdPath = file;

    /* Windows 下显示成反斜杠路径，看着更自然 */
    ui->lineEdit_filename->setText(QDir::toNativeSeparators(file));

    appendText(QString("=== 已加载 %1：波特率 %2，REQ 报文 %3 条 ===")
                   .arg(QFileInfo(file).fileName(), cmds.first())
                   .arg(cmds.size() - 1),
               false);
}

/* ============================================================
 *  清除窗口（你原来就有的槽）
 * ============================================================ */
void MainWindow::on_pushButton_clear_clicked()
{
    ui->textEdit->clear();
}
