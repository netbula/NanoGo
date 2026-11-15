#pragma once
#include "Review.h"
#include <QWidget>

// 胜率曲线：横轴是手数，纵轴是黑方胜率（另画一条淡色的目差线）。
// 坏棋用红点（恶手）、橙点（疑问手）标出，点曲线跳到那一手。
class WinrateGraph : public QWidget {
    Q_OBJECT
public:
    explicit WinrateGraph(QWidget *parent = nullptr);
    void setData(const QList<GameNode *> &line, const QList<MoveEval> &evals, const GameNode *current, int boardSize);

signals:
    void nodeClicked(GameNode *node);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    QSize sizeHint() const override { return {600, 150}; }
    QSize minimumSizeHint() const override { return {200, 90}; }

private:
    int indexAt(double x) const;
    QRectF plotRect() const;

    QList<GameNode *> m_line;
    QList<MoveEval> m_evals;
    const GameNode *m_current = nullptr;
    int m_size = 19;
};
