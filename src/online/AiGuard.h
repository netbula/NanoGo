#pragma once
#include <QStringList>

// 防作弊：找这台电脑上正在运行的其他围棋 AI（按进程名）。只看进程名，不读文件、不截屏。
namespace AiGuard {
// 返回找到的程序名（去重）。自己这个 NanoGo 进程不算；另开的 NanoGo 算。
QStringList findGoAi();
}
