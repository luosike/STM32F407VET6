#ifndef MYCOMBOBOX_H
#define MYCOMBOBOX_H


#pragma once
#include <QComboBox>

class MyComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit MyComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

signals:
    void popupAboutToShow(); // 自定义的展开前信号

public slots:
    void showPopup() override {
        emit popupAboutToShow(); // 先发出信号，便于在展开前动态刷新数据
        QComboBox::showPopup();  // 再调用基类实现弹出下拉列表
    }
};
#endif // MYCOMBOBOX_H
