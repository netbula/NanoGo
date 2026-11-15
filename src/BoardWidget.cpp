#include "BoardWidget.h"
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

static const char *kCols = "ABCDEFGHJKLMNOPQRSTUVWXYZ";

BoardWidget::BoardWidget(GameTree *tree, QWidget *parent) : QWidget(parent), m_tree(tree)
{
    setMouseTracking(true);
    setMinimumSize(400, 400);
}

QString BoardWidget::toGtp(int x, int y, int size)
{
    if (x < 0) return QStringLiteral("pass");
    return QString(QChar(kCols[x])) + QString::number(size - y);
}

bool BoardWidget::fromGtp(const QString &s, int size, int *x, int *y)
{
    if (s.isEmpty() || s.compare("pass", Qt::CaseInsensitive) == 0) return false;
    const char *c = strchr(kCols, s[0].toUpper().toLatin1());
    if (!c) return false;
    *x = int(c - kCols);
    *y = size - s.mid(1).toInt();
    return *x >= 0 && *x < size && *y >= 0 && *y < size;
}

QPointF BoardWidget::pointToPixel(int x, int y) const
{
    return {m_left + x * m_cell, m_top + y * m_cell};
}

bool BoardWidget::pixelToPoint(QPointF p, int *x, int *y) const
{
    if (m_cell <= 0) return false;
    *x = int(std::lround((p.x() - m_left) / m_cell));
    *y = int(std::lround((p.y() - m_top) / m_cell));
    return *x >= 0 && *y >= 0 && *x < m_tree->boardSize() && *y < m_tree->boardSize();
}

void BoardWidget::drawStone(QPainter &p, QPointF c, Stone s, double r, double alpha) const
{
    p.save();
    p.setOpacity(alpha);
    QRadialGradient g(c - QPointF(r * 0.35, r * 0.35), r * 1.3);
    if (s == Stone::Black) { g.setColorAt(0, QColor(90, 90, 90)); g.setColorAt(1, QColor(10, 10, 10)); }
    else { g.setColorAt(0, QColor(255, 255, 255)); g.setColorAt(1, QColor(200, 200, 195)); }
    p.setPen(QPen(QColor(0, 0, 0, 120), 0.8));
    p.setBrush(g);
    p.drawEllipse(c, r, r);
    p.restore();
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int n = m_tree->boardSize();
    const double side = qMin(width(), height());
    m_cell = side / (n + 1);
    m_left = (width() - side) / 2 + m_cell;
    m_top = (height() - side) / 2 + m_cell;

    // 棋盘底色
    QRectF boardRect(m_left - m_cell * 0.8, m_top - m_cell * 0.8, m_cell * (n - 1 + 1.6), m_cell * (n - 1 + 1.6));
    p.fillRect(rect(), palette().window());
    p.fillRect(boardRect, QColor(220, 179, 92));

    // 网格
    p.setPen(QPen(QColor(40, 30, 10), 1.0));
    for (int i = 0; i < n; ++i) {
        p.drawLine(pointToPixel(i, 0), pointToPixel(i, n - 1));
        p.drawLine(pointToPixel(0, i), pointToPixel(n - 1, i));
    }
    // 星位
    QList<int> stars;
    if (n == 19) stars = {3, 9, 15}; else if (n == 13) stars = {3, 6, 9}; else if (n == 9) stars = {2, 4, 6};
    p.setBrush(QColor(40, 30, 10));
    for (int sx : stars) for (int sy : stars) {
        if (n == 9 && (sx == 4) != (sy == 4)) continue;
        p.drawEllipse(pointToPixel(sx, sy), m_cell * 0.1, m_cell * 0.1);
    }
    // 坐标
    QFont f = font();
    f.setPixelSize(qMax(9, int(m_cell * 0.32)));
    p.setFont(f);
    p.setPen(QColor(60, 45, 20));
    for (int i = 0; i < n; ++i) {
        QPointF top = pointToPixel(i, 0) - QPointF(0, m_cell * 0.62);
        p.drawText(QRectF(top.x() - m_cell / 2, top.y() - m_cell / 4, m_cell, m_cell / 2), Qt::AlignCenter, QString(QChar(kCols[i])));
        QPointF left = pointToPixel(0, i) - QPointF(m_cell * 0.62, 0);
        p.drawText(QRectF(left.x() - m_cell / 2, left.y() - m_cell / 4, m_cell, m_cell / 2), Qt::AlignCenter, QString::number(n - i));
    }

    const Board &b = m_tree->board();
    const double r = m_cell * 0.47;
    const QJsonObject raw = m_tree->current()->analysis.raw;

    // 形势判断（地盘归属）
    if (m_showOwnership && raw.contains("ownership")) {
        const QJsonArray own = raw.value("ownership").toArray();
        if (own.size() == n * n) {
            for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
                double v = own[y * n + x].toDouble(); // 黑方视角，+1 = 黑地
                if (std::abs(v) < 0.15) continue;
                QColor c = v > 0 ? QColor(0, 0, 0) : QColor(255, 255, 255);
                c.setAlphaF(qMin(0.75, std::abs(v) * 0.75));
                p.fillRect(QRectF(pointToPixel(x, y) - QPointF(m_cell * 0.25, m_cell * 0.25), QSizeF(m_cell * 0.5, m_cell * 0.5)), c);
            }
        }
    }

    // 棋子
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
        Stone s = b.at(x, y);
        if (s != Stone::Empty) drawStone(p, pointToPixel(x, y), s, r);
    }
    // 形势：死子上画对方颜色的小方块
    if (m_showOwnership && raw.contains("ownership")) {
        const QJsonArray own = raw.value("ownership").toArray();
        if (own.size() == n * n) {
            for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
                const Stone s = b.at(x, y);
                const double v = own[y * n + x].toDouble();
                if ((s == Stone::Black && v < -0.5) || (s == Stone::White && v > 0.5)) {
                    QColor c = v > 0 ? QColor(0, 0, 0, 200) : QColor(255, 255, 255, 220);
                    p.setPen(QPen(v > 0 ? Qt::white : Qt::black, 0.8));
                    p.setBrush(c);
                    p.drawRect(QRectF(pointToPixel(x, y) - QPointF(m_cell * 0.2, m_cell * 0.2), QSizeF(m_cell * 0.4, m_cell * 0.4)));
                }
            }
        }
    }

    // 坏棋标记：棋子右上角的小三角（红 = 恶手，橙 = 疑问手）
    for (auto it = m_moveMarks.cbegin(); it != m_moveMarks.cend(); ++it) {
        const int x = it.key() % n, y = it.key() / n;
        if (b.at(x, y) == Stone::Empty) continue;
        const QPointF c = pointToPixel(x, y) + QPointF(r * 0.55, -r * 0.55);
        const double s = qMax(4.0, r * 0.42);
        QPolygonF tri({c + QPointF(-s, -s * 0.6), c + QPointF(s, -s * 0.6), c + QPointF(0, s)});
        p.setPen(QPen(Qt::white, 1));
        p.setBrush(it.value() == 2 ? QColor(220, 50, 50) : QColor(240, 160, 30));
        p.drawPolygon(tri);
    }

    // 最后一手标记
    const Move &last = m_tree->current()->move;
    if (last.isValid() && !last.isPass()) {
        p.setPen(QPen(last.color == Stone::Black ? Qt::white : Qt::black, qMax(1.5, m_cell * 0.06)));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pointToPixel(last.x, last.y), r * 0.5, r * 0.5);
    }

    // 候选点 + 变化图（鼠标悬停的优先，其次是右侧选中的参考图）
    if ((m_showCandidates || !m_pinnedPv.isEmpty()) && raw.contains("moveInfos")) {
        const QJsonArray infos = raw.value("moveInfos").toArray();
        int totalVisits = 0;
        for (const auto &v : infos) totalVisits = qMax(totalVisits, v.toObject().value("visits").toInt());
        QJsonArray hoverPv;
        for (const auto &v : infos) {
            QJsonObject mi = v.toObject();
            int x, y;
            if (!fromGtp(mi.value("move").toString(), n, &x, &y)) continue;
            if (m_showCandidates && QPoint(x, y) == m_hover) hoverPv = mi.value("pv").toArray();
        }
        if (hoverPv.isEmpty()) for (const QString &mv : m_pinnedPv) hoverPv.append(mv);
        if (!hoverPv.isEmpty()) {
            Stone c = m_tree->toPlay();
            int num = 1;
            for (const auto &mv : hoverPv) {
                int x, y;
                if (fromGtp(mv.toString(), n, &x, &y) && b.at(x, y) == Stone::Empty) {
                    drawStone(p, pointToPixel(x, y), c, r, 0.85);
                    p.setPen(c == Stone::Black ? Qt::white : Qt::black);
                    QFont nf = font(); nf.setPixelSize(int(m_cell * 0.42)); nf.setBold(true); p.setFont(nf);
                    p.drawText(QRectF(pointToPixel(x, y) - QPointF(r, r), QSizeF(2 * r, 2 * r)), Qt::AlignCenter, QString::number(num));
                }
                ++num;
                c = opponent(c);
                if (num > 30) break;
            }
        } else if (m_showCandidates) {
            double bestScore = -1e9;
            for (const auto &v : infos) bestScore = qMax(bestScore, v.toObject().value("scoreLead").toDouble());
            for (const auto &v : infos) {
                QJsonObject mi = v.toObject();
                int x, y;
                if (!fromGtp(mi.value("move").toString(), n, &x, &y)) continue;
                const int visits = mi.value("visits").toInt();
                if (totalVisits > 0 && visits < totalVisits * 0.02 && mi.value("order").toInt() > 0) continue;
                // KataGo 返回的是黑方视角；换成当前走棋方视角
                const bool blackToPlay = m_tree->toPlay() == Stone::Black;
                double wr = mi.value("winrate").toDouble();
                double sc = mi.value("scoreLead").toDouble();
                double best = bestScore;
                if (!blackToPlay) { wr = 1 - wr; }
                double loss = blackToPlay ? best - sc : 0;
                if (!blackToPlay) {
                    double minScore = 1e9;
                    for (const auto &w : infos) minScore = qMin(minScore, w.toObject().value("scoreLead").toDouble());
                    loss = sc - minScore;
                    sc = -sc;
                }
                QColor col = mi.value("order").toInt() == 0 ? QColor(60, 160, 255)
                           : loss < 1 ? QColor(60, 200, 90) : loss < 3 ? QColor(240, 210, 60)
                           : loss < 6 ? QColor(245, 140, 40) : QColor(220, 60, 60);
                col.setAlpha(210);
                p.setPen(Qt::NoPen);
                p.setBrush(col);
                QPointF c = pointToPixel(x, y);
                p.drawEllipse(c, r, r);
                p.setPen(Qt::black);
                QFont tf = font(); tf.setPixelSize(qMax(8, int(m_cell * 0.27)));
                p.setFont(tf);
                p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, r), Qt::AlignHCenter | Qt::AlignBottom, QString::number(wr * 100, 'f', 0));
                p.drawText(QRectF(c.x() - r, c.y(), 2 * r, r), Qt::AlignHCenter | Qt::AlignTop,
                           (sc >= 0 ? "+" : "") + QString::number(sc, 'f', 1));
            }
        }
    }

    // 悬停预览
    if (m_hover.x() >= 0 && b.at(m_hover.x(), m_hover.y()) == Stone::Empty) {
        drawStone(p, pointToPixel(m_hover.x(), m_hover.y()), m_tree->toPlay(), r, 0.35);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent *e)
{
    int x, y;
    if (e->button() == Qt::LeftButton && pixelToPoint(e->position(), &x, &y)) emit pointClicked(x, y);
}

void BoardWidget::mouseMoveEvent(QMouseEvent *e)
{
    int x, y;
    QPoint h(-1, -1);
    if (pixelToPoint(e->position(), &x, &y)) h = QPoint(x, y);
    if (h != m_hover) { m_hover = h; update(); }
}

void BoardWidget::leaveEvent(QEvent *)
{
    m_hover = QPoint(-1, -1);
    update();
}
