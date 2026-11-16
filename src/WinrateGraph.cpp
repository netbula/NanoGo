#include "WinrateGraph.h"
#include "BoardWidget.h"
#include "I18n.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

WinrateGraph::WinrateGraph(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void WinrateGraph::setData(const QList<GameNode *> &line, const QList<MoveEval> &evals, const GameNode *current, int boardSize)
{
    m_size = boardSize;
    m_line = line;
    m_evals = evals;
    m_current = current;
    update();
}

// 横轴至少显示 50 手，短棋局不会被拉得很宽
int WinrateGraph::span() const { return qMax(50, int(m_line.size()) - 1); }

QRectF WinrateGraph::plotRect() const { return QRectF(34, 8, width() - 44, height() - 24); }

int WinrateGraph::indexAt(double x) const
{
    if (m_line.size() < 2) return -1;
    const QRectF r = plotRect();
    const double t = (x - r.left()) / r.width();
    return qBound(0, int(t * span() + 0.5), int(m_line.size()) - 1);
}

void WinrateGraph::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    const QRectF r = plotRect();
    const int n = span();
    auto xOf = [&](int i) { return r.left() + r.width() * i / n; };

    // 背景：上半黑优、下半白优
    p.fillRect(QRectF(r.left(), r.top(), r.width(), r.height() / 2), QColor(0, 0, 0, 14));
    p.setPen(QPen(QColor(150, 150, 150), 1, Qt::DashLine));
    p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));
    p.setPen(palette().color(QPalette::Text));
    QFont f = font(); f.setPointSizeF(f.pointSizeF() * 0.85); p.setFont(f);
    p.drawText(QRectF(0, r.top() - 6, 30, 14), Qt::AlignRight | Qt::AlignVCenter, T("黑"));
    p.drawText(QRectF(0, r.center().y() - 7, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "50%");
    p.drawText(QRectF(0, r.bottom() - 8, 30, 14), Qt::AlignRight | Qt::AlignVCenter, T("白"));
    // 手数刻度
    for (int m = 50; m < n; m += 50)
        p.drawText(QRectF(xOf(m) - 20, r.bottom() + 2, 40, 14), Qt::AlignCenter, QString::number(m));

    bool any = false;
    for (const GameNode *nd : m_line) if (nd->analysis.valid) { any = true; break; }
    if (m_line.size() < 2 || !any) {
        p.drawText(r, Qt::AlignCenter, T("走几步或打开棋谱后，点“分析整盘棋”就能看到胜率曲线"));
        return;
    }

    // 目差线（淡橙色，按最大目差缩放）
    double maxLead = 10;
    for (const GameNode *nd : m_line) if (nd->analysis.valid) maxLead = qMax(maxLead, qAbs(nd->analysis.scoreLeadBlack));
    QPainterPath lead, wr;
    bool leadStarted = false, wrStarted = false;
    for (int i = 0; i < m_line.size(); ++i) {
        const Analysis &a = m_line[i]->analysis;
        if (!a.valid) continue;  // 缺数据的手跳过，前后直接连起来
        const QPointF pl(xOf(i), r.center().y() - a.scoreLeadBlack / maxLead * r.height() / 2);
        const QPointF pw(xOf(i), r.bottom() - a.winrateBlack * r.height());
        if (leadStarted) lead.lineTo(pl); else { lead.moveTo(pl); leadStarted = true; }
        if (wrStarted) wr.lineTo(pw); else { wr.moveTo(pw); wrStarted = true; }
    }
    p.setPen(QPen(QColor(230, 150, 50, 170), 1.5));
    p.drawPath(lead);
    p.setPen(QPen(QColor(40, 90, 170), 2.2));
    p.drawPath(wr);

    // 坏棋点
    for (const MoveEval &e : m_evals) {
        if (!e.known || e.severity == 0) continue;
        const Analysis &a = e.node->analysis;
        const QPointF c(xOf(e.moveNo), r.bottom() - a.winrateBlack * r.height());
        p.setPen(QPen(Qt::white, 1));
        p.setBrush(e.severity == 2 ? QColor(220, 50, 50) : QColor(240, 160, 30));
        p.drawEllipse(c, e.severity == 2 ? 4.5 : 3.5, e.severity == 2 ? 4.5 : 3.5);
    }

    // 当前手
    const int cur = m_line.indexOf(const_cast<GameNode *>(m_current));
    if (cur >= 0) {
        p.setPen(QPen(QColor(60, 160, 90), 1.5));
        p.drawLine(QPointF(xOf(cur), r.top()), QPointF(xOf(cur), r.bottom()));
    }
}

void WinrateGraph::mousePressEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().x());
    if (i >= 0) emit nodeClicked(m_line[i]);
}

void WinrateGraph::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton) { mousePressEvent(e); return; }
    const int i = indexAt(e->position().x());
    if (i < 0) return;
    const Analysis &a = m_line[i]->analysis;
    QString tip = i == 0 ? T("开局") : T("第 %1 手").arg(i);
    const Move &m = m_line[i]->move;
    if (i > 0 && m.isValid())
        tip += QStringLiteral(" %1 %2").arg(m.color == Stone::Black ? T("黑") : T("白"),
                                             m.isPass() ? T("停一手") : BoardWidget::toGtp(m.x, m.y, m_size));
    if (a.valid) tip += "\n" + T("黑胜率 %1%，%2 领先 %3 目").arg(a.winrateBlack * 100, 0, 'f', 1)
                                     .arg(a.scoreLeadBlack >= 0 ? T("黑") : T("白")).arg(qAbs(a.scoreLeadBlack), 0, 'f', 1);
    if (i > 0 && i - 1 < m_evals.size() && m_evals[i - 1].known) {
        const MoveEval &ev = m_evals[i - 1];
        tip += "\n" + T("这一手损失 %1 目（胜率 %2%）").arg(ev.scoreLoss, 0, 'f', 1).arg(ev.winLoss * 100, 0, 'f', 1);
        if (ev.severity == 2) tip += " · " + T("恶手");
        else if (ev.severity == 1) tip += " · " + T("疑问手");
    }
    QToolTip::showText(e->globalPosition().toPoint(), tip, this);
}
