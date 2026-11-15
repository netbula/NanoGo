#include "TsumegoEditor.h"
#include "BoardWidget.h"
#include "I18n.h"
#include "KataGoEngine.h"
#include "Sgf.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QVBoxLayout>

namespace {
QString colorStr(Stone s) { return s == Stone::White ? QStringLiteral("W") : QStringLiteral("B"); }
}

TsumegoEditor::TsumegoEditor(KataGoEngine *engine, QWidget *parent)
    : QDialog(parent), m_engine(engine)
{
    setWindowTitle(T("死活题编辑器"));
    resize(1100, 760);
    auto *root = new QHBoxLayout(this);

    // 左：题目列表
    auto *left = new QVBoxLayout;
    m_fileInfo = new QLabel(this);
    m_fileInfo->setWordWrap(true);
    left->addWidget(m_fileInfo);
    m_listW = new QListWidget(this);
    connect(m_listW, &QListWidget::currentRowChanged, this, [this](int r) { if (r >= 0 && r != m_index) showProblem(r); });
    left->addWidget(m_listW, 1);
    auto *lb = new QGridLayout;
    auto btn = [this](const QString &t, auto fn) {
        auto *b = new QPushButton(t, this);
        connect(b, &QPushButton::clicked, this, fn);
        return b;
    };
    lb->addWidget(btn(T("新题"), [this] { newProblem(); }), 0, 0);
    lb->addWidget(btn(T("复制"), [this] { duplicateProblem(); }), 0, 1);
    lb->addWidget(btn(T("删除"), [this] { deleteProblem(); }), 0, 2);
    lb->addWidget(btn(T("上移"), [this] { moveProblem(-1); }), 1, 0);
    lb->addWidget(btn(T("下移"), [this] { moveProblem(1); }), 1, 1);
    left->addLayout(lb);
    auto *fb = new QGridLayout;
    fb->addWidget(btn(T("新建题库"), [this] {
        if (!maybeSave()) return;
        setCollection({}, {});
    }), 0, 0);
    fb->addWidget(btn(T("打开…"), [this] { openFile(); }), 0, 1);
    fb->addWidget(btn(T("保存"), [this] { save(); }), 1, 0);
    fb->addWidget(btn(T("另存为…"), [this] { saveAs(); }), 1, 1);
    left->addLayout(fb);
    root->addLayout(left, 1);

    // 中：棋盘
    m_board = new BoardWidget(&m_tree, this);
    m_board->setShowCandidates(false);
    connect(m_board, &BoardWidget::pointClicked, this, [this](int x, int y) { edit(x, y, false); });
    connect(m_board, &BoardWidget::pointRightClicked, this, [this](int x, int y) { edit(x, y, true); });
    root->addWidget(m_board, 3);

    // 右：工具和题目信息
    auto *right = new QVBoxLayout;
    auto *tools = new QGroupBox(T("摆子"), this);
    auto *tl = new QVBoxLayout(tools);
    m_toolBlack = new QRadioButton(T("黑子"), tools);
    m_toolWhite = new QRadioButton(T("白子"), tools);
    m_toolAlt = new QRadioButton(T("黑白交替"), tools);
    m_toolErase = new QRadioButton(T("擦除"), tools);
    m_toolBlack->setChecked(true);
    for (auto *r : {m_toolBlack, m_toolWhite, m_toolAlt, m_toolErase}) tl->addWidget(r);
    auto *hint = new QLabel(T("左键摆子，再点一次拿掉；右键随时擦除。"), tools);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray;");
    tl->addWidget(hint);
    tl->addWidget(btn(T("清空棋盘"), [this] {
        cur().black.clear();
        cur().white.clear();
        m_dirty = true;
        syncBoard();
    }));
    right->addWidget(tools);

    auto *info = new QGroupBox(T("题目"), this);
    auto *form = new QFormLayout(info);
    m_title = new QLineEdit(info);
    m_title->setPlaceholderText(T("可以不填"));
    connect(m_title, &QLineEdit::textEdited, this, [this](const QString &t) {
        cur().title = t;
        m_dirty = true;
        refreshList();
    });
    form->addRow(T("题目名："), m_title);
    m_toPlay = new QComboBox(info);
    m_toPlay->addItem(T("黑先"), int(Stone::Black));
    m_toPlay->addItem(T("白先"), int(Stone::White));
    connect(m_toPlay, &QComboBox::activated, this, [this] {
        cur().toPlay = Stone(m_toPlay->currentData().toInt());
        m_dirty = true;
        syncBoard();
        refreshList();
    });
    form->addRow(T("谁先走："), m_toPlay);
    m_goal = new QComboBox(info);
    m_goal->addItem(T("活"), "live");
    m_goal->addItem(T("杀"), "kill");
    m_goal->addItem(T("劫"), "ko");
    m_goal->addItem(T("吃子"), "capture");
    m_goal->addItem(T("不指定"), "");
    connect(m_goal, &QComboBox::activated, this, [this] {
        cur().goal = m_goal->currentData().toString();
        m_dirty = true;
        refreshList();
    });
    form->addRow(T("目标："), m_goal);
    right->addWidget(info);

    auto *chk = new QGroupBox(T("检查"), this);
    auto *cl = new QVBoxLayout(chk);
    cl->addWidget(btn(T("用 KataGo 检查这道题"), [this] { check(); }));
    m_checkInfo = new QLabel(chk);
    m_checkInfo->setWordWrap(true);
    m_checkInfo->setTextFormat(Qt::RichText);
    m_checkInfo->setText(T("<span style='color:gray'>检查：KataGo 找出正解第一手，并确认这道题“下不下大不一样”。</span>"));
    cl->addWidget(m_checkInfo);
    right->addWidget(chk);

    right->addStretch(1);
    right->addWidget(btn(T("做这套题"), [this] {
        if (m_list.isEmpty()) return;
        emit practiceRequested(m_list, m_path, m_index);
    }));
    right->addWidget(btn(T("关闭"), [this] { reject(); }));
    root->addLayout(right, 1);

    connect(m_engine, &KataGoEngine::analysisReceived, this, &TsumegoEditor::onAnalysis);
    setCollection({}, {});
}

void TsumegoEditor::setCollection(const QList<TsumegoProblem> &list, const QString &path)
{
    m_list = list;
    m_path = path;
    m_dirty = false;
    if (m_list.isEmpty()) m_list.append(TsumegoProblem{});
    m_index = 0;
    refreshList();
    showProblem(0);
}

void TsumegoEditor::refreshList()
{
    m_listW->blockSignals(true);
    m_listW->clear();
    for (int i = 0; i < m_list.size(); ++i) {
        const TsumegoProblem &p = m_list[i];
        QString t = QStringLiteral("%1. %2").arg(i + 1).arg(Tsumego::goalText(p));
        if (!p.title.isEmpty()) t += " · " + p.title;
        if (p.black.isEmpty() && p.white.isEmpty()) t += T("（空）");
        m_listW->addItem(t);
    }
    m_listW->setCurrentRow(m_index);
    m_listW->blockSignals(false);
    m_fileInfo->setText((m_path.isEmpty() ? T("<b>新题库</b>（还没保存）") : T("<b>%1</b>").arg(QFileInfo(m_path).fileName().toHtmlEscaped()))
                        + (m_dirty ? T("　*有改动") : QString()) + T("<br>共 %1 题").arg(m_list.size()));
}

void TsumegoEditor::showProblem(int i)
{
    m_index = qBound(0, i, int(m_list.size()) - 1);
    const TsumegoProblem &p = m_list[m_index];
    m_title->setText(p.title);
    m_toPlay->setCurrentIndex(p.toPlay == Stone::White ? 1 : 0);
    m_goal->setCurrentIndex(qMax(0, m_goal->findData(p.goal)));
    m_checkInfo->setText(T("<span style='color:gray'>检查：KataGo 找出正解第一手，并确认这道题“下不下大不一样”。</span>"));
    m_board->setPinnedPv({});
    syncBoard();
    refreshList();
}

void TsumegoEditor::syncBoard()
{
    Sgf::load(Tsumego::toSgf(cur()), m_tree, nullptr);
    m_board->setPinnedPv({});
    m_board->update();
    refreshList();
}

void TsumegoEditor::edit(int x, int y, bool erase)
{
    TsumegoProblem &p = cur();
    const QPoint pt(x, y);
    const bool hadB = p.black.removeAll(pt) > 0;
    const bool hadW = p.white.removeAll(pt) > 0;
    if (!erase && !m_toolErase->isChecked()) {
        Stone s = m_toolBlack->isChecked() ? Stone::Black : m_toolWhite->isChecked() ? Stone::White
                : (m_altNext ? Stone::Black : Stone::White);
        // 点在同色子上 = 拿掉它
        const bool same = (s == Stone::Black && hadB) || (s == Stone::White && hadW);
        if (!same) {
            (s == Stone::Black ? p.black : p.white).append(pt);
            if (m_toolAlt->isChecked()) m_altNext = !m_altNext;
        }
    }
    m_dirty = true;
    syncBoard();
}

void TsumegoEditor::newProblem()
{
    TsumegoProblem p;
    if (!m_list.isEmpty()) { p.toPlay = cur().toPlay; p.goal = cur().goal; }
    m_list.insert(m_index + 1, p);
    m_dirty = true;
    showProblem(m_index + 1);
}

void TsumegoEditor::duplicateProblem()
{
    m_list.insert(m_index + 1, cur());
    m_dirty = true;
    showProblem(m_index + 1);
}

void TsumegoEditor::deleteProblem()
{
    if (m_list.size() <= 1) {
        m_list[0] = TsumegoProblem{};
    } else {
        if (QMessageBox::question(this, T("删除"), T("删除第 %1 题？").arg(m_index + 1)) != QMessageBox::Yes) return;
        m_list.removeAt(m_index);
    }
    m_dirty = true;
    showProblem(qMin(m_index, int(m_list.size()) - 1));
}

void TsumegoEditor::moveProblem(int dir)
{
    const int j = m_index + dir;
    if (j < 0 || j >= m_list.size()) return;
    m_list.swapItemsAt(m_index, j);
    m_dirty = true;
    showProblem(j);
}

void TsumegoEditor::openFile()
{
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, T("打开题库"), QFileInfo(m_path).absolutePath(),
                                                      T("SGF 题目 (*.sgf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, T("打开题库"), T("无法打开文件：%1").arg(path));
        return;
    }
    const QList<TsumegoProblem> list = Tsumego::loadCollection(Sgf::decode(f.readAll()));
    if (list.isEmpty()) {
        QMessageBox::warning(this, T("打开题库"), T("这个文件里没有找到死活题（需要有摆好的黑白子）。"));
        return;
    }
    setCollection(list, path);
}

bool TsumegoEditor::save()
{
    if (m_path.isEmpty()) return saveAs();
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(Tsumego::saveCollection(m_list).toUtf8()) < 0 || !f.commit()) {
        QMessageBox::warning(this, T("保存"), T("无法保存到：%1").arg(m_path));
        return false;
    }
    m_dirty = false;
    refreshList();
    return true;
}

bool TsumegoEditor::saveAs()
{
    const QString path = QFileDialog::getSaveFileName(this, T("保存题库"), m_path.isEmpty() ? T("我的死活题.sgf") : m_path,
                                                      T("SGF 题目 (*.sgf)"));
    if (path.isEmpty()) return false;
    m_path = path.endsWith(".sgf", Qt::CaseInsensitive) ? path : path + ".sgf";
    return save();
}

bool TsumegoEditor::maybeSave()
{
    if (!m_dirty) return true;
    const auto ans = QMessageBox::question(this, T("死活题编辑器"), T("题库有改动，要保存吗？"),
                                           QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ans == QMessageBox::Cancel) return false;
    if (ans == QMessageBox::Save) return save();
    return true;
}

void TsumegoEditor::reject()
{
    if (!maybeSave()) return;
    if (!m_bestId.isEmpty()) m_engine->terminate(m_bestId);
    if (!m_tenukiId.isEmpty()) m_engine->terminate(m_tenukiId);
    m_bestId.clear();
    m_tenukiId.clear();
    QDialog::reject();
}

// 检查：最佳一手之后的归属 和 “不下（停一手）对方应过之后”的归属差得多，题目才成立
void TsumegoEditor::check()
{
    const TsumegoProblem &p = cur();
    if (p.black.isEmpty() || p.white.isEmpty()) {
        m_checkInfo->setText(T("<span style='color:#d33'>请先摆上黑子和白子。</span>"));
        return;
    }
    if (m_engine->state() != KataGoEngine::State::Ready) {
        m_checkInfo->setText(T("<span style='color:#d33'>引擎还没就绪，请稍候再试。</span>"));
        return;
    }
    const int n = p.size;
    m_rect = Tsumego::region(p, 1);
    m_allow = QJsonArray();
    for (int y = m_rect.top(); y <= m_rect.bottom(); ++y)
        for (int x = m_rect.left(); x <= m_rect.right(); ++x) m_allow.append(BoardWidget::toGtp(x, y, n));
    m_allow.append(QStringLiteral("pass"));
    QJsonArray stones;
    for (const QPoint &q : p.black) stones.append(QJsonArray{"B", BoardWidget::toGtp(q.x(), q.y(), n)});
    for (const QPoint &q : p.white) stones.append(QJsonArray{"W", BoardWidget::toGtp(q.x(), q.y(), n)});
    auto base = [&](Stone mover) {
        QJsonObject q;
        q["rules"] = "chinese";
        q["komi"] = 0;
        q["boardXSize"] = n;
        q["boardYSize"] = n;
        q["initialStones"] = stones;
        q["maxVisits"] = 800;
        q["includeOwnership"] = true;
        q["overrideSettings"] = QJsonObject{{"wideRootNoise", 0.0}};
        q["allowMoves"] = QJsonArray{QJsonObject{{"player", colorStr(mover)}, {"moves", m_allow}, {"untilDepth", 1}}};
        return q;
    };
    QJsonObject qb = base(p.toPlay);
    qb["initialPlayer"] = colorStr(p.toPlay);
    qb["moves"] = QJsonArray();
    qb["includeMovesOwnership"] = true;
    QJsonObject qt = base(opponent(p.toPlay));
    qt["moves"] = QJsonArray{QJsonArray{colorStr(p.toPlay), "pass"}};
    m_bestResp = QJsonObject();
    m_tenukiResp = QJsonObject();
    m_bestId = m_engine->analyze(qb);
    m_tenukiId = m_engine->analyze(qt);
    m_checkInfo->setText(T("<span style='color:gray'>KataGo 正在检查…</span>"));
}

void TsumegoEditor::onAnalysis(const QJsonObject &resp)
{
    const QString id = resp.value("id").toString();
    if (id.isEmpty() || (id != m_bestId && id != m_tenukiId)) return;
    if (resp.value("isDuringSearch").toBool()) return;
    if (id == m_bestId) { m_bestResp = resp; m_bestId.clear(); }
    else { m_tenukiId.clear(); m_tenukiResp = resp; }
    if (!m_bestId.isEmpty() || !m_tenukiId.isEmpty()) return;

    const TsumegoProblem &p = cur();
    const int n = p.size;
    const QJsonObject best = m_bestResp.value("moveInfos").toArray().first().toObject();
    const QJsonArray ob = best.value("ownership").toArray();
    const QJsonArray ot = m_tenukiResp.value("ownership").toArray();
    if (ob.size() != n * n || ot.size() != n * n) return;
    double diff = 0;
    for (int y = m_rect.top(); y <= m_rect.bottom(); ++y)
        for (int x = m_rect.left(); x <= m_rect.right(); ++x) diff += qAbs(ob[y * n + x].toDouble() - ot[y * n + x].toDouble());
    const QString move = best.value("move").toString();
    QStringList pv;
    for (const auto &v : best.value("pv").toArray()) {
        int px, py;
        if (!BoardWidget::fromGtp(v.toString(), n, &px, &py) || !m_rect.contains(px, py) || pv.size() >= 12) break;
        pv << v.toString();
    }
    m_board->setPinnedPv(pv);

    // 目标对不对得上：下了正解以后，“活”= 自己的子活，“杀”= 对方的子死
    const double sign = p.toPlay == Stone::White ? -1 : 1; // 归属是黑方视角
    auto ownedByMover = [&](const QList<QPoint> &stones) {
        int good = 0;
        for (const QPoint &q : stones)
            if (m_rect.contains(q) && ob[q.y() * n + q.x()].toDouble() * sign > 0.5) ++good;
        return good;
    };
    const QList<QPoint> &mine = p.toPlay == Stone::White ? p.white : p.black;
    const QList<QPoint> &theirs = p.toPlay == Stone::White ? p.black : p.white;
    QString goalNote;
    if (p.goal == "live" && ownedByMover(mine) < mine.size() * 0.8)
        goalNote = T("<br><span style='color:#b70'>注意：KataGo 认为下了这手后，先走方的棋好像还不能完全活。</span>");
    else if (p.goal == "kill" && ownedByMover(theirs) < theirs.size() * 0.5)
        goalNote = T("<br><span style='color:#b70'>注意：KataGo 认为下了这手后，对方的棋好像还没被杀死。</span>");

    if (move.compare("pass", Qt::CaseInsensitive) == 0 || diff < 4)
        m_checkInfo->setText(T("<span style='color:#d33'><b>这道题可能不成立</b></span>：不管先走方下不下，结果都差不多（变化量 %1）。请检查棋子是不是摆对了。")
                                 .arg(diff, 0, 'f', 1) + goalNote);
    else
        m_checkInfo->setText(T("<span style='color:#2a7'><b>题目成立</b></span>：KataGo 的正解第一手是 <b>%1</b>（棋盘上用数字显示了正解变化）。下不下的差别：%2。")
                                 .arg(move).arg(diff, 0, 'f', 1) + goalNote);
}
