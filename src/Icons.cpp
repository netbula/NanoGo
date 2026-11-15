#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QLinearGradient>
#include <functional>

namespace {
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
    p.setPen(QPen(black ? QColor(0x10, 0x10, 0x10) : QColor(0x9a, 0x9a, 0x9a), 0.8));
    p.drawEllipse(c, r, r);
}

} // namespace

namespace Icons {

QIcon app()
{
    return make([](QPainter &p) {
        // 圆角方块 + 木纹渐变棋盘 + 一黑一白两颗大棋子
        QLinearGradient wood(0, 0, 64, 64);
        wood.setColorAt(0, QColor(0xf2, 0xc9, 0x7d));
        wood.setColorAt(1, QColor(0xd4, 0x98, 0x3e));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 40));
        p.drawRoundedRect(QRectF(3, 5, 58, 58), 13, 13); // 阴影
        p.setBrush(wood);
        p.drawRoundedRect(QRectF(3, 2, 58, 58), 13, 13);
        // 3×3 交叉点，黑白子各占一个点，互不接触（棋子必须落在交叉点上）
        p.setPen(QPen(QColor(0x7a, 0x52, 0x1a, 170), 1.6));
        for (double v : {14.0, 32.0, 50.0}) {
            p.drawLine(QPointF(9, v - 1), QPointF(55, v - 1));
            p.drawLine(QPointF(v, 8), QPointF(v, 54));
        }
        stone(p, QPointF(14, 49), 8.2, true);
        stone(p, QPointF(32, 31), 8.2, false);
        stone(p, QPointF(50, 31), 8.2, true);
    });
}

// 工具栏图示来自 Lucide 图标库（https://lucide.dev ，ISC 许可证），预先按颜色渲染成 PNG 放在 res/icons
static QIcon fromRes(const char *name)
{
    QIcon icon;
    for (int px : {32, 64}) icon.addFile(QStringLiteral(":/icons/%1_%2.png").arg(QLatin1String(name)).arg(px), QSize(px, px));
    return icon;
}

QIcon aiGame() { return fromRes("aiGame"); }
QIcon first() { return fromRes("first"); }
QIcon back() { return fromRes("back"); }
QIcon forward() { return fromRes("forward"); }
QIcon last() { return fromRes("last"); }
QIcon pass() { return fromRes("pass"); }
QIcon aiMove() { return fromRes("aiMove"); }
QIcon swap() { return fromRes("swap"); }
QIcon analyze() { return fromRes("analyze"); }
QIcon candidates() { return fromRes("candidates"); }
QIcon territory() { return fromRes("territory"); }
QIcon reference() { return fromRes("reference"); }
QIcon count() { return fromRes("count"); }
QIcon tsumego() { return fromRes("tsumego"); }

} // namespace Icons
