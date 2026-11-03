#include "MyComboBox.h"

MyComboBox::MyComboBox(QWidget *parent) : QComboBox(parent)
{
}

void MyComboBox::showPopup()
{
    emit popupAboutToShow(); // 先发信号
    QComboBox::showPopup();  // 再弹窗
}