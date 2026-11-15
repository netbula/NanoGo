#pragma once
#include <QString>

// 界面多语言：源码里写中文，T() 按当前语言查表返回英语 / 日语 / 韩语，查不到就用中文
namespace I18n {
void setLanguage(const QString &code); // zh / en / ja / ko
QString language();
}
QString T(const char *zh);
