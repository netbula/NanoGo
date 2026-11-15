#include "I18n.h"
#include "Sgf.h"
#include <QStringDecoder>
#include <QRegularExpression>

namespace {

struct Parser {
    const QString &s;
    int pos = 0;
    QString error;
    int size = 19;

    explicit Parser(const QString &text) : s(text) {}

    void skipWs() { while (pos < s.size() && s[pos].isSpace()) ++pos; }

    QPoint coord(const QString &v) const
    {
        if (v.size() < 2) return QPoint(-1, -1);
        int x = v[0].unicode() - 'a', y = v[1].unicode() - 'a';
        if (x < 0 || y < 0 || x >= size || y >= size) return QPoint(-1, -1); // "tt" 等表示停一手
        return QPoint(x, y);
    }

    QList<QPoint> coordList(const QStringList &vals) const
    {
        QList<QPoint> out;
        for (const QString &v : vals) {
            if (v.contains(':')) { // 矩形压缩写法 aa:cc
                QPoint a = coord(v.section(':', 0, 0)), b = coord(v.section(':', 1, 1));
                for (int y = qMin(a.y(), b.y()); y <= qMax(a.y(), b.y()); ++y)
                    for (int x = qMin(a.x(), b.x()); x <= qMax(a.x(), b.x()); ++x) out.append(QPoint(x, y));
            } else {
                QPoint p = coord(v);
                if (p.x() >= 0) out.append(p);
            }
        }
        return out;
    }

    bool readNode(GameNode *node, GameTree &tree, bool isRoot)
    {
        // 当前位置在 ';' 之后
        while (true) {
            skipWs();
            if (pos >= s.size()) return true;
            QChar c = s[pos];
            if (!c.isLetter()) return true;
            QString ident;
            while (pos < s.size() && s[pos].isLetter()) { if (s[pos].isUpper()) ident += s[pos]; ++pos; }
            QStringList vals;
            skipWs();
            while (pos < s.size() && s[pos] == '[') {
                ++pos;
                QString v;
                while (pos < s.size() && s[pos] != ']') {
                    if (s[pos] == '\\' && pos + 1 < s.size()) ++pos;
                    v += s[pos++];
                }
                if (pos >= s.size()) { error = T("SGF 文件不完整：缺少 ]"); return false; }
                ++pos;
                vals.append(v);
                skipWs();
            }
            if (vals.isEmpty()) continue;
            if (isRoot && ident == "SZ") {
                size = vals[0].section(':', 0, 0).toInt();
                if (size < 2 || size > 25) { error = T("不支持的棋盘大小：%1").arg(vals[0]); return false; }
            } else if (isRoot && ident == "KM") tree.setKomi(vals[0].toDouble());
            else if (isRoot && ident == "PB") tree.playerBlack = vals[0];
            else if (isRoot && ident == "PW") tree.playerWhite = vals[0];
            else if (isRoot && ident == "RE") tree.result = vals[0];
            else if (isRoot && ident == "RU") {
                QString r = vals[0].toLower();
                if (r.contains("japan")) tree.setRules("japanese");
                else if (r.contains("korea")) tree.setRules("korean");
                else if (r.contains("aga")) tree.setRules("aga");
                else if (r.contains("tromp") || r.contains("tt")) tree.setRules("tromp-taylor");
                else tree.setRules("chinese");
            }
            else if (ident == "B" || ident == "W") {
                node->move.color = ident == "B" ? Stone::Black : Stone::White;
                QPoint p = coord(vals[0]);
                node->move.x = p.x(); node->move.y = p.y();
            }
            else if (ident == "AB") node->addBlack += coordList(vals);
            else if (ident == "AW") node->addWhite += coordList(vals);
            else if (ident == "C") node->comment = vals[0];
            else if (ident != "FF" && ident != "GM" && ident != "CA" && ident != "AP") node->extraProps[ident] = vals;
        }
    }

    // 读一个 "(...)" 序列，挂在 parent 下面
    bool readSequence(GameNode *parent, GameTree &tree, GameNode **rootOut)
    {
        skipWs();
        if (pos >= s.size() || s[pos] != '(') { error = T("SGF 格式错误：缺少 ("); return false; }
        ++pos;
        GameNode *cur = parent;
        while (true) {
            skipWs();
            if (pos >= s.size()) { error = T("SGF 文件不完整：缺少 )"); return false; }
            QChar c = s[pos];
            if (c == ';') {
                ++pos;
                GameNode *n;
                if (!cur) { n = new GameNode; *rootOut = n; }
                else { n = new GameNode; n->parent = cur; cur->children.append(n); }
                if (!readNode(n, tree, cur == nullptr)) return false;
                cur = n;
            } else if (c == '(') {
                if (!cur) { error = T("SGF 格式错误"); return false; }
                if (!readSequence(cur, tree, rootOut)) return false;
            } else if (c == ')') {
                ++pos;
                return true;
            } else {
                ++pos; // 忽略杂字符
            }
        }
    }
};

QString escape(const QString &v)
{
    QString out = v;
    out.replace("\\", "\\\\");
    out.replace("]", "\\]");
    return out;
}

QString coordStr(int x, int y)
{
    if (x < 0) return QString();
    return QString(QChar('a' + x)) + QChar('a' + y);
}

void writeNode(const GameNode *n, QString &out)
{
    out += ';';
    if (n->move.isValid())
        out += (n->move.color == Stone::Black ? "B[" : "W[") + coordStr(n->move.x, n->move.y) + "]";
    if (!n->addBlack.isEmpty()) { out += "AB"; for (auto p : n->addBlack) out += "[" + coordStr(p.x(), p.y()) + "]"; }
    if (!n->addWhite.isEmpty()) { out += "AW"; for (auto p : n->addWhite) out += "[" + coordStr(p.x(), p.y()) + "]"; }
    for (auto it = n->extraProps.cbegin(); it != n->extraProps.cend(); ++it) {
        out += it.key();
        for (const QString &v : it.value()) out += "[" + escape(v) + "]";
    }
    if (!n->comment.isEmpty()) out += "C[" + escape(n->comment) + "]";
}

void writeTree(const GameNode *n, QString &out)
{
    // 写当前节点及其后续；有多个子节点时每个分支用括号
    const GameNode *cur = n;
    while (true) {
        writeNode(cur, out);
        if (cur->children.size() == 1) { cur = cur->children.first(); continue; }
        for (const GameNode *c : cur->children) { out += '('; writeTree(c, out); out += ")\n"; }
        break;
    }
}

} // namespace

QString Sgf::decode(const QByteArray &bytes)
{
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(bytes);
    if (!utf8.hasError()) return text;
    QStringDecoder gb("GB18030");
    if (gb.isValid()) return gb.decode(bytes);
    return QString::fromLatin1(bytes);
}

bool Sgf::load(const QString &text, GameTree &tree, QString *error)
{
    int start = text.indexOf('(');
    if (start < 0) { if (error) *error = T("这不是 SGF 棋谱文件"); return false; }
    Parser p(text);
    p.pos = start;
    GameNode *root = nullptr;
    tree.setKomi(7.5);
    tree.setRules("chinese");
    tree.playerBlack.clear(); tree.playerWhite.clear(); tree.result.clear();
    // SZ 必须在解析坐标前确定：先粗略扫描一次
    QRegularExpression szRe(QStringLiteral("SZ\\[(\\d+)"));
    auto m = szRe.match(text);
    if (m.hasMatch()) p.size = m.captured(1).toInt();
    if (!p.readSequence(nullptr, tree, &root) || !root) {
        delete root;
        if (error) *error = p.error.isEmpty() ? T("无法解析棋谱") : p.error;
        return false;
    }
    tree.adoptRoot(root, p.size);
    return true;
}

QString Sgf::save(const GameTree &tree)
{
    QString out = QStringLiteral("(;GM[1]FF[4]CA[UTF-8]AP[NanoGo:0.1]SZ[%1]KM[%2]")
                      .arg(tree.boardSize()).arg(tree.komi());
    const QString ru = tree.rules() == "japanese" ? "Japanese" : tree.rules() == "korean" ? "Korean"
                     : tree.rules() == "aga" ? "AGA" : tree.rules() == "tromp-taylor" ? "Tromp-Taylor" : "Chinese";
    out += "RU[" + ru + "]";
    if (!tree.playerBlack.isEmpty()) out += "PB[" + escape(tree.playerBlack) + "]";
    if (!tree.playerWhite.isEmpty()) out += "PW[" + escape(tree.playerWhite) + "]";
    if (!tree.result.isEmpty()) out += "RE[" + escape(tree.result) + "]";
    // 根节点本身的内容（去掉开头的 ';'）
    QString rootStr;
    writeTree(tree.root(), rootStr);
    out += rootStr.mid(1);
    out += ")\n";
    return out;
}
