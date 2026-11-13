#pragma once
#include "Board.h"
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QMap>
#include <memory>

// 一步棋：颜色 + 位置。pass 用 x=y=-1 表示。
struct Move {
    Stone color = Stone::Empty;
    int x = -1, y = -1;
    bool isPass() const { return x < 0; }
    bool isValid() const { return color != Stone::Empty; }
};

// 引擎对某个局面的分析结果（缓存在节点上，供复盘曲线使用）
struct Analysis {
    bool valid = false;
    double winrateBlack = 0.5;   // 黑方胜率
    double scoreLeadBlack = 0.0; // 黑方领先目数
    int visits = 0;
    QJsonObject raw;             // KataGo 原始返回
};

class GameNode {
public:
    GameNode *parent = nullptr;
    QList<GameNode *> children;
    Move move;
    QList<QPoint> addBlack, addWhite; // 摆子 (AB/AW)
    QString comment;
    QMap<QString, QStringList> extraProps; // 其他 SGF 属性，原样保存
    Analysis analysis;

    ~GameNode() { qDeleteAll(children); }
    GameNode *addChild(const Move &m);
    int depth() const;
};

// 整盘棋：根节点信息 + 当前所在节点 + 根据路径计算出的棋盘
class GameTree {
public:
    GameTree(int size = 19, double komi = 7.5);
    ~GameTree();

    int boardSize() const { return m_size; }
    double komi() const { return m_komi; }
    void setKomi(double k) { m_komi = k; }
    QString rules() const { return m_rules; }
    void setRules(const QString &r) { m_rules = r; }
    QString playerBlack, playerWhite, result;

    GameNode *root() const { return m_root; }
    GameNode *current() const { return m_current; }
    void setCurrent(GameNode *n);

    const Board &board() const { return m_board; }
    Stone toPlay() const { return m_toPlay; }
    QPoint koPoint() const { return m_ko; }

    // 在当前局面落子；已有相同变化则直接走进去。非法着返回 false。
    bool play(int x, int y);
    bool pass();
    bool back();
    bool forward();
    void toStart();
    void toEnd();

    // 从根到当前节点的着法序列（给引擎用）
    QList<GameNode *> pathToCurrent() const;
    QList<GameNode *> mainLineFromCurrentPath() const;

    // 把根节点交给 GameTree 管理（读 SGF 用）
    void adoptRoot(GameNode *root, int size);

private:
    void rebuild();

    int m_size;
    double m_komi;
    QString m_rules = QStringLiteral("chinese");
    GameNode *m_root;
    GameNode *m_current;
    Board m_board;
    Stone m_toPlay = Stone::Black;
    QPoint m_ko{-1, -1};
};
