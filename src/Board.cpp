#include "Board.h"

Board::Board(int size) : m_size(size), m_cells(size * size, Stone::Empty) {}

int Board::liberties(int x, int y, QVector<int> *group) const
{
    const Stone color = at(x, y);
    QVector<char> seen(m_size * m_size, 0);
    QVector<int> stack{y * m_size + x};
    seen[y * m_size + x] = 1;
    int libs = 0;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!stack.isEmpty()) {
        int idx = stack.takeLast();
        if (group) group->append(idx);
        int cx = idx % m_size, cy = idx / m_size;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (!onBoard(nx, ny)) continue;
            int n = ny * m_size + nx;
            if (seen[n]) continue;
            seen[n] = 1;
            Stone s = m_cells[n];
            if (s == Stone::Empty) ++libs;
            else if (s == color) stack.append(n);
        }
    }
    return libs;
}

void Board::removeGroup(const QVector<int> &group)
{
    for (int idx : group) m_cells[idx] = Stone::Empty;
}

bool Board::isLegal(int x, int y, Stone color, QPoint koPoint) const
{
    Board copy = *this;
    return copy.play(x, y, color, koPoint);
}

bool Board::play(int x, int y, Stone color, QPoint koPoint, int *captured, QPoint *newKo)
{
    if (!onBoard(x, y) || at(x, y) != Stone::Empty) return false;
    if (koPoint == QPoint(x, y)) return false;

    set(x, y, color);
    const Stone opp = opponent(color);
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int total = 0;
    QPoint lastCaptured(-1, -1);
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        if (!onBoard(nx, ny) || at(nx, ny) != opp) continue;
        QVector<int> group;
        if (liberties(nx, ny, &group) == 0) {
            total += group.size();
            if (group.size() == 1) lastCaptured = QPoint(group[0] % m_size, group[0] / m_size);
            removeGroup(group);
        }
    }
    QVector<int> own;
    int ownLibs = liberties(x, y, &own);
    if (ownLibs == 0) { // 自杀，禁止
        set(x, y, Stone::Empty);
        return false;
    }
    if (color == Stone::Black) m_capBlack += total; else m_capWhite += total;
    if (captured) *captured = total;
    if (newKo) {
        // 单子提单子且落下的子只有一口气 -> 产生劫
        *newKo = (total == 1 && own.size() == 1 && ownLibs == 1) ? lastCaptured : QPoint(-1, -1);
    }
    return true;
}
