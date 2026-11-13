#pragma once
#include <QVector>
#include <QPoint>

enum class Stone : char { Empty = 0, Black = 1, White = 2 };

inline Stone opponent(Stone s) { return s == Stone::Black ? Stone::White : (s == Stone::White ? Stone::Black : Stone::Empty); }

// 棋盘状态：落子、提子、禁着点判断
class Board {
public:
    explicit Board(int size = 19);

    int size() const { return m_size; }
    Stone at(int x, int y) const { return m_cells[y * m_size + x]; }
    void set(int x, int y, Stone s) { m_cells[y * m_size + x] = s; }
    bool onBoard(int x, int y) const { return x >= 0 && y >= 0 && x < m_size && y < m_size; }

    // 尝试落子。成功返回 true，并把提掉的子数写入 captured。
    // koPoint 是当前禁止回提的位置（没有则为 (-1,-1)）。
    bool play(int x, int y, Stone color, QPoint koPoint, int *captured = nullptr, QPoint *newKo = nullptr);
    bool isLegal(int x, int y, Stone color, QPoint koPoint) const;

    int capturesBlack() const { return m_capBlack; } // 黑方提掉的白子数
    int capturesWhite() const { return m_capWhite; }

private:
    int liberties(int x, int y, QVector<int> *group) const;
    void removeGroup(const QVector<int> &group);

    int m_size;
    QVector<Stone> m_cells;
    int m_capBlack = 0;
    int m_capWhite = 0;
};
