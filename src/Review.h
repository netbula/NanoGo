#pragma once
#include "GameTree.h"
#include <QList>

// 复盘：根据每个局面的胜率/目差，算出每一手损失了多少，标出坏棋
struct MoveEval {
    GameNode *node = nullptr;
    int moveNo = 0;          // 第几手（从 1 开始）
    Stone color = Stone::Empty;
    double winLoss = 0;      // 这一手让自己的胜率掉了多少（0~1）
    double scoreLoss = 0;    // 这一手让自己少了多少目
    int severity = 0;        // 0 正常，1 疑问手，2 恶手
    bool known = false;      // 前后两个局面都分析过才算得出来
};

namespace Review {
// 判定标准：损失 ≥ 5 目或胜率掉 ≥ 15% 为恶手；≥ 2 目或 ≥ 7% 为疑问手
int severity(double winLoss, double scoreLoss);
// line[0] 是根节点，line[i] 是第 i 手之后的局面
QList<MoveEval> evaluate(const QList<GameNode *> &line);
}
