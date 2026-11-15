#pragma once
#include "GameTree.h"
#include <QHash>
#include <QWidget>

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(GameTree *tree, QWidget *parent = nullptr);
    void setShowCandidates(bool on) { m_showCandidates = on; update(); }
    bool showCandidates() const { return m_showCandidates; }
    void setShowOwnership(bool on) { m_showOwnership = on; update(); }
    bool showOwnership() const { return m_showOwnership; }
    // 固定显示一条参考图（变化图），空列表 = 不显示
    void setPinnedPv(const QStringList &pv) { m_pinnedPv = pv; update(); }
    QStringList pinnedPv() const { return m_pinnedPv; }
    // 坏棋标记：key = y*n+x，value = 1 疑问手 / 2 恶手
    void setMoveMarks(const QHash<int, int> &marks) { m_moveMarks = marks; update(); }

    // 只显示这一块（棋盘坐标，左上角为 0,0），空 = 整个棋盘
    void setViewRect(const QRect &r) { m_view = r; update(); }
    QRect visibleRect() const;

    static QString toGtp(int x, int y, int size);
    static bool fromGtp(const QString &s, int size, int *x, int *y);

signals:
    void pointClicked(int x, int y);
    void pointRightClicked(int x, int y);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;
    QSize sizeHint() const override { return {760, 760}; }

private:
    bool pixelToPoint(QPointF p, int *x, int *y) const;
    QPointF pointToPixel(int x, int y) const;
    void drawStone(QPainter &p, QPointF c, Stone s, double r, double alpha = 1.0) const;

    GameTree *m_tree;
    double m_cell = 0, m_left = 0, m_top = 0;
    QPoint m_hover{-1, -1};
    bool m_showCandidates = true;
    bool m_showOwnership = false;
    QStringList m_pinnedPv;
    QHash<int, int> m_moveMarks;
    QRect m_view;
};
