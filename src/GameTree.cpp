#include "GameTree.h"
#include <algorithm>

GameNode *GameNode::addChild(const Move &m)
{
    auto *n = new GameNode;
    n->parent = this;
    n->move = m;
    children.append(n);
    return n;
}

int GameNode::depth() const
{
    int d = 0;
    for (const GameNode *p = parent; p; p = p->parent) ++d;
    return d;
}

GameTree::GameTree(int size, double komi)
    : m_size(size), m_komi(komi), m_root(new GameNode), m_current(m_root), m_board(size) {}

GameTree::~GameTree() { delete m_root; }

void GameTree::adoptRoot(GameNode *root, int size)
{
    delete m_root;
    m_root = root;
    m_size = size;
    m_current = root;
    rebuild();
}

QList<GameNode *> GameTree::pathToCurrent() const
{
    QList<GameNode *> path;
    for (GameNode *n = m_current; n; n = n->parent) path.prepend(n);
    return path;
}

QList<GameNode *> GameTree::mainLineFromCurrentPath() const
{
    // 当前路径 + 从当前节点沿第一个分支走到底
    QList<GameNode *> line = pathToCurrent();
    for (GameNode *n = m_current; !n->children.isEmpty();) {
        n = n->children.first();
        line.append(n);
    }
    return line;
}

void GameTree::setCurrent(GameNode *n)
{
    m_current = n;
    rebuild();
}

void GameTree::rebuild()
{
    m_board = Board(m_size);
    m_ko = QPoint(-1, -1);
    m_toPlay = Stone::Black;
    for (GameNode *n : pathToCurrent()) {
        for (const QPoint &p : n->addBlack) m_board.set(p.x(), p.y(), Stone::Black);
        for (const QPoint &p : n->addWhite) m_board.set(p.x(), p.y(), Stone::White);
        if (n->move.isValid()) {
            if (n->move.isPass()) {
                m_ko = QPoint(-1, -1);
            } else {
                QPoint ko;
                if (!m_board.play(n->move.x, n->move.y, n->move.color, QPoint(-1, -1), nullptr, &ko)) {
                    // SGF 里的非法着：直接摆上，不报错
                    m_board.set(n->move.x, n->move.y, n->move.color);
                    ko = QPoint(-1, -1);
                }
                m_ko = ko;
            }
            m_toPlay = opponent(n->move.color);
        } else if (!n->addWhite.isEmpty() && n->addBlack.isEmpty() && n == m_root) {
            m_toPlay = Stone::Black;
        }
    }
    // 让子棋：根节点只有黑子摆放时轮白走
    if (m_current == m_root && !m_root->addBlack.isEmpty() && m_root->addWhite.isEmpty())
        m_toPlay = Stone::White;
}

bool GameTree::play(int x, int y)
{
    for (GameNode *c : m_current->children) {
        if (c->move.color == m_toPlay && c->move.x == x && c->move.y == y) {
            setCurrent(c);
            return true;
        }
    }
    if (!m_board.isLegal(x, y, m_toPlay, m_ko)) return false;
    setCurrent(m_current->addChild(Move{m_toPlay, x, y}));
    return true;
}

bool GameTree::pass()
{
    for (GameNode *c : m_current->children) {
        if (c->move.color == m_toPlay && c->move.isPass()) { setCurrent(c); return true; }
    }
    setCurrent(m_current->addChild(Move{m_toPlay, -1, -1}));
    return true;
}

bool GameTree::back()
{
    if (!m_current->parent) return false;
    setCurrent(m_current->parent);
    return true;
}

bool GameTree::forward()
{
    if (m_current->children.isEmpty()) return false;
    setCurrent(m_current->children.first());
    return true;
}

void GameTree::toStart() { setCurrent(m_root); }

void GameTree::toEnd()
{
    GameNode *n = m_current;
    while (!n->children.isEmpty()) n = n->children.first();
    setCurrent(n);
}
