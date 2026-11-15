#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <functional>

namespace {
const QColor kInk(0x33, 0x3a, 0x45);      // 线条色，深浅背景上都清楚
const QColor kAccent(0x2b, 0x7a, 0xd6);   // 蓝色强调
const QColor kGreen(0x3c, 0xb3, 0x71);
const QColor kWood(0xe3, 0xb4, 0x5c);

QIcon make(const std::function<void(QPainter &)> &draw)
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 64.0, size / 64.0); // 所有图标都按 64×64 的坐标画
        draw(p);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

void stone(QPainter &p, QPointF c, double r, bool black)
{
    QRadialGradient g(c - QPointF(r * 0.35, r * 0.35), r * 1.3);
    if (black) { g.setColorAt(0, QColor(0x66, 0x66, 0x66)); g.setColorAt(1, QColor(0x10, 0x10, 0x10)); }
    else { g.setColorAt(0, Qt::white); g.setColorAt(1, QColor(0xc8, 0xc8, 0xc8)); }
    p.setBrush(g);
    p.setPen(QPen(black ? QColor(0x10, 0x10, 0x10) : QColor(0x80, 0x80, 0x80), 1.5));
    p.drawEllipse(c, r, r);
}

QPen ink(double w = 5) { return QPen(kInk, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin); }

void triangle(QPainter &p, double x, double dir) // dir = +1 向右，-1 向左
{
    QPainterPath path;
    path.moveTo(x, 14);
    path.lineTo(x + dir * 20, 32);
    path.lineTo(x, 50);
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    p.drawPath(path);
}

void miniBoard(QPainter &p, QRectF r, int lines)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kWood);
    p.drawRoundedRect(r, 5, 5);
    p.setPen(QPen(QColor(0x5a, 0x40, 0x10), 1.6));
    const double m = r.width() / (lines + 1);
    for (int i = 1; i <= lines; ++i) {
        p.drawLine(QPointF(r.left() + m, r.top() + i * m), QPointF(r.right() - m, r.top() + i * m));
        p.drawLine(QPointF(r.left() + i * m, r.top() + m), QPointF(r.left() + i * m, r.bottom() - m));
    }
}
} // namespace

namespace Icons {

QIcon app()
{
    return make([](QPainter &p) {
        miniBoard(p, QRectF(3, 3, 58, 58), 5);
        const double m = 58.0 / 6;
        stone(p, QPointF(3 + 2 * m, 3 + 2 * m), 8.5, true);
        stone(p, QPointF(3 + 4 * m, 3 + 3 * m), 8.5, false);
        stone(p, QPointF(3 + 3 * m, 3 + 4 * m), 8.5, true);
    });
}

QIcon aiGame()
{
    return make([](QPainter &p) {
        stone(p, QPointF(22, 36), 16, true);
        stone(p, QPointF(42, 28), 16, false);
        // 小芯片表示 AI
        p.setPen(QPen(kAccent, 3));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(34, 40, 24, 18), 3, 3);
        p.setPen(kAccent);
        QFont f; f.setPixelSize(12); f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(34, 40, 24, 18), Qt::AlignCenter, "AI");
    });
}

QIcon first()
{
    return make([](QPainter &p) {
        p.setPen(ink(6));
        p.drawLine(QPointF(12, 14), QPointF(12, 50));
        triangle(p, 36, -1);
        triangle(p, 54, -1);
    });
}

QIcon back() { return make([](QPainter &p) { triangle(p, 42, -1); }); }
QIcon forward() { return make([](QPainter &p) { triangle(p, 22, 1); }); }

QIcon last()
{
    return make([](QPainter &p) {
        triangle(p, 10, 1);
        triangle(p, 28, 1);
        p.setPen(ink(6));
        p.drawLine(QPointF(52, 14), QPointF(52, 50));
    });
}

QIcon pass()
{
    return make([](QPainter &p) {
        // 一只手掌：表示“这一手不下”
        p.setPen(ink(4));
        p.setBrush(QColor(0xf3, 0xd9, 0xb1));
        QPainterPath hand;
        hand.addRoundedRect(QRectF(16, 28, 32, 28), 10, 10);
        for (int i = 0; i < 4; ++i) hand.addRoundedRect(QRectF(16 + i * 8, 8 + (i == 0 || i == 3 ? 6 : 0), 8, 28), 4, 4);
        p.drawPath(hand.simplified());
    });
}

QIcon aiMove()
{
    return make([](QPainter &p) {
        stone(p, QPointF(26, 38), 17, true);
        // 闪电：让 AI 马上走
        QPainterPath bolt;
        bolt.moveTo(44, 4); bolt.lineTo(34, 26); bolt.lineTo(44, 26);
        bolt.lineTo(38, 46); bolt.lineTo(58, 20); bolt.lineTo(47, 20); bolt.lineTo(54, 4);
        bolt.closeSubpath();
        p.setPen(QPen(QColor(0xb0, 0x70, 0x00), 2));
        p.setBrush(QColor(0xff, 0xc4, 0x2e));
        p.drawPath(bolt);
    });
}

QIcon swap()
{
    return make([](QPainter &p) {
        stone(p, QPointF(18, 32), 12, true);
        stone(p, QPointF(46, 32), 12, false);
        p.setPen(QPen(kAccent, 4, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(14, 6, 36, 24), 30 * 16, 120 * 16);
        p.drawArc(QRectF(14, 34, 36, 24), 210 * 16, 120 * 16);
        p.setBrush(kAccent);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF({QPointF(47, 4), QPointF(52, 16), QPointF(41, 14)}));
        p.drawPolygon(QPolygonF({QPointF(17, 60), QPointF(12, 48), QPointF(23, 50)}));
    });
}

QIcon analyze()
{
    return make([](QPainter &p) {
        // 胜率曲线 + 放大镜
        p.setPen(QPen(kAccent, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(6, 44), QPointF(16, 30), QPointF(26, 36), QPointF(38, 16)}));
        p.setPen(ink(5));
        p.setBrush(QColor(255, 255, 255, 160));
        p.drawEllipse(QPointF(38, 34), 13, 13);
        p.drawLine(QPointF(47, 44), QPointF(58, 56));
    });
}

QIcon candidates()
{
    return make([](QPainter &p) {
        miniBoard(p, QRectF(4, 4, 56, 56), 3);
        p.setPen(QPen(QColor(0x1d, 0x6b, 0x3f), 1.5));
        p.setBrush(kGreen);
        p.drawEllipse(QPointF(18, 18), 8, 8);
        p.drawEllipse(QPointF(46, 32), 8, 8);
        p.setBrush(kAccent);
        p.setPen(QPen(QColor(0x1a, 0x4d, 0x8c), 1.5));
        p.drawEllipse(QPointF(32, 46), 8, 8);
    });
}

QIcon territory()
{
    return make([](QPainter &p) {
        miniBoard(p, QRectF(4, 4, 56, 56), 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRect(QRectF(8, 8, 22, 48));
        p.setBrush(QColor(255, 255, 255, 190));
        p.drawRect(QRectF(34, 8, 22, 48));
    });
}

QIcon reference()
{
    return make([](QPainter &p) {
        stone(p, QPointF(20, 22), 14, true);
        stone(p, QPointF(44, 42), 14, false);
        QFont f; f.setPixelSize(16); f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(6, 8, 28, 28), Qt::AlignCenter, "1");
        p.setPen(Qt::black);
        p.drawText(QRectF(30, 28, 28, 28), Qt::AlignCenter, "2");
    });
}

QIcon count()
{
    return make([](QPainter &p) {
        // 计数板 + 对勾：点目、结束对局
        p.setPen(ink(4));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(8, 6, 40, 52), 5, 5);
        p.setPen(ink(3));
        for (int i = 0; i < 4; ++i) p.drawLine(QPointF(15, 18 + i * 9), QPointF(41, 18 + i * 9));
        p.setPen(QPen(kGreen, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(34, 44), QPointF(42, 52), QPointF(58, 32)}));
    });
}

} // namespace Icons
