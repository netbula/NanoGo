#pragma once
#include "Board.h"
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>

// 死活题：一个局面（摆好的黑白子）+ 谁先走 + 目标。答案由 KataGo 现场判断，题目不需要带答案树
struct TsumegoProblem {
    QString title;           // 题目名（SGF 的 GN 或注释第一行）
    QList<QPoint> black, white;
    Stone toPlay = Stone::Black;
    QString goal;            // live / kill / ko / 空（不知道）
    int size = 19;
};

namespace Tsumego {
// 读取一个 SGF 题库：文件里可以有多盘（多个 "(;...)"），每盘一道题
QList<TsumegoProblem> loadCollection(const QString &sgfText);
// 程序自带的基础死活题
QList<TsumegoProblem> builtin();
// 题目所在区域（所有棋子的外接矩形向外扩 margin 格，靠近边时一直扩到边）
QRect region(const TsumegoProblem &p, int margin);
// 生成这道题的 SGF（用来装进棋盘）
QString toSgf(const TsumegoProblem &p);
// “黑先活”之类的说明
QString goalText(const TsumegoProblem &p);
}
