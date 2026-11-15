#pragma once
#include <QIcon>

// 全部图标都用 QPainter 自己画（原创，不依赖图片文件和第三方图标库）
namespace Icons {
QIcon app();
QIcon aiGame();
QIcon first();
QIcon back();
QIcon forward();
QIcon last();
QIcon pass();
QIcon aiMove();
QIcon swap();
QIcon analyze();
QIcon candidates();
QIcon territory();
QIcon reference();
QIcon count();
}
