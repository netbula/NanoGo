#include "Tsumego.h"
#include "GameTree.h"
#include "I18n.h"
#include "Sgf.h"
#include <QFile>

namespace {
// 按括号层次把 SGF 文本切成一盘一盘（跳过 [] 里的内容）
QStringList splitGames(const QString &text)
{
    QStringList games;
    int depth = 0, start = -1;
    bool inValue = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (inValue) {
            if (c == '\\') ++i;
            else if (c == ']') inValue = false;
            continue;
        }
        if (c == '[') inValue = true;
        else if (c == '(') { if (depth++ == 0) start = i; }
        else if (c == ')' && depth > 0) {
            if (--depth == 0 && start >= 0) { games << text.mid(start, i - start + 1); start = -1; }
        }
    }
    return games;
}
}

QList<TsumegoProblem> Tsumego::loadCollection(const QString &sgfText)
{
    QList<TsumegoProblem> out;
    for (const QString &g : splitGames(sgfText)) {
        GameTree tree;
        if (!Sgf::load(g, tree, nullptr)) continue;
        GameNode *n = tree.root();
        // 有的题库把摆子放在第二个节点
        if (n->addBlack.isEmpty() && n->addWhite.isEmpty() && !n->children.isEmpty()) n = n->children.first();
        if (n->addBlack.isEmpty() && n->addWhite.isEmpty()) continue;
        TsumegoProblem p;
        p.size = tree.boardSize();
        p.black = n->addBlack;
        p.white = n->addWhite;
        tree.setCurrent(n);
        p.toPlay = tree.toPlay();
        // 没写 PL 时：多数题库是第一个答案节点的颜色
        if (!tree.root()->extraProps.contains("PL") && !n->extraProps.contains("PL") && !n->children.isEmpty()
            && n->children.first()->move.isValid())
            p.toPlay = n->children.first()->move.color;
        if (n->extraProps.contains("PL")) p.toPlay = n->extraProps.value("PL").value(0).startsWith('W') ? Stone::White : Stone::Black;
        p.goal = tree.root()->extraProps.value("NG").value(0);
        p.title = tree.root()->extraProps.value("GN").value(0);
        if (p.title.isEmpty()) p.title = tree.root()->comment.section('\n', 0, 0).left(40);
        out << p;
    }
    return out;
}

QList<TsumegoProblem> Tsumego::builtin()
{
    QFile f(QStringLiteral(":/tsumego/basic.sgf"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return loadCollection(QString::fromUtf8(f.readAll()));
}

QRect Tsumego::region(const TsumegoProblem &p, int margin)
{
    int x0 = p.size, y0 = p.size, x1 = -1, y1 = -1;
    for (const QList<QPoint> *l : {&p.black, &p.white})
        for (const QPoint &q : *l) {
            x0 = qMin(x0, q.x()); y0 = qMin(y0, q.y());
            x1 = qMax(x1, q.x()); y1 = qMax(y1, q.y());
        }
    if (x1 < 0) return QRect(0, 0, p.size, p.size);
    x0 -= margin; y0 -= margin; x1 += margin; y1 += margin;
    // 离边只差一两路时干脆扩到边，看得清角和边
    if (x0 <= 2) x0 = 0;
    if (y0 <= 2) y0 = 0;
    if (x1 >= p.size - 3) x1 = p.size - 1;
    if (y1 >= p.size - 3) y1 = p.size - 1;
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
}

QString Tsumego::toSgf(const TsumegoProblem &p)
{
    auto c = [](const QPoint &q) { return QStringLiteral("[%1%2]").arg(QChar('a' + q.x())).arg(QChar('a' + q.y())); };
    QString s = QStringLiteral("(;GM[1]FF[4]SZ[%1]KM[0]RU[Chinese]PL[%2]").arg(p.size).arg(p.toPlay == Stone::White ? "W" : "B");
    if (!p.black.isEmpty()) { s += "AB"; for (const QPoint &q : p.black) s += c(q); }
    if (!p.white.isEmpty()) { s += "AW"; for (const QPoint &q : p.white) s += c(q); }
    return s + ")";
}

QString Tsumego::goalText(const TsumegoProblem &p)
{
    const bool b = p.toPlay != Stone::White;
    if (p.goal == "live") return b ? T("黑先活") : T("白先活");
    if (p.goal == "kill") return b ? T("黑先杀") : T("白先杀");
    if (p.goal == "ko") return b ? T("黑先劫") : T("白先劫");
    if (p.goal == "capture") return b ? T("黑先吃子") : T("白先吃子");
    return b ? T("黑先") : T("白先");
}
