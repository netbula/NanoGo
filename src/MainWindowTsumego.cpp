// 死活题练习：题目只给局面，对错由 KataGo 现场判断
//
// 判题方法：轮到你时先让 KataGo 在题目范围内找出最好的一手，记下“下了这手以后题目范围内每个点归谁”；
// 你落子后，让 KataGo 替对方在题目范围内找最强的应手，再看一次归属。
// 两次归属差得多（例如本该活的棋死了），就是下错了；差不多，就是对的。
// 这样任何 SGF 题目都能做，不需要题目自带答案。
#include "I18n.h"
#include "MainWindow.h"
#include "BoardWidget.h"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QCryptographicHash>
#include <QDir>
#include "Sgf.h"
#include "TsumegoEditor.h"

namespace {
constexpr double kWrongThreshold = 4.0;  // 题目范围内归属变化总和超过这个数就算错（约等于 2 个点换了主人）
QString colorStr(Stone s) { return s == Stone::White ? QStringLiteral("W") : QStringLiteral("B"); }
}

QString MainWindow::tsProgressGroup() const
{
    return QString::fromLatin1(QCryptographicHash::hash(m_tsKey.toUtf8(), QCryptographicHash::Md5).toHex().left(12));
}

void MainWindow::enterTsumego()
{
    if (m_tsMode) return;
    if (m_tsProblems.isEmpty()) {
        m_tsProblems = Tsumego::builtin();
        m_tsKey = QStringLiteral("builtin");
        m_tsName = T("基础死活（内置）");
    }
    if (m_tsProblems.isEmpty()) {
        QMessageBox::warning(this, T("死活题"), T("没有找到题目。"));
        m_tsAct->setChecked(false);
        return;
    }
    // 先保存当前棋局，做完题回来恢复
    autosave();
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_aiNode = nullptr;
    m_engine->terminateAll();
    m_reviewQueryId.clear();
    m_reviewLine.clear();
    m_side.clear();
    m_ownQueryId.clear();
    m_queryId.clear();
    m_tsWasAnalyzing = m_analyzeAct->isChecked();
    m_tsWasCandidates = m_candAct->isChecked();
    m_analyzeAct->setChecked(false);
    m_analyzeAct->setEnabled(false);
    m_candAct->setChecked(false);
    m_ownAct->setChecked(false);
    m_tsMode = true;
    m_tsAct->setChecked(true);
    m_reviewPanel->hide();
    m_tsPanel->show();
    m_leftSplit->setSizes({700, 0, 140});
    QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
    loadTsProblem(st.value(tsProgressGroup() + "/last", 0).toInt());
}

void MainWindow::exitTsumego()
{
    if (!m_tsMode) return;
    m_engine->terminateAll();
    m_tsBestId.clear();
    m_tsReplyId.clear();
    m_tsMode = false;
    m_tsAct->setChecked(false);
    m_tsPanel->hide();
    m_reviewPanel->show();
    m_leftSplit->setSizes({700, 170, 0});
    m_board->setViewRect({});
    m_board->setPinnedPv({});
    m_analyzeAct->setEnabled(true);
    m_candAct->setChecked(m_tsWasCandidates);
    restoreAutosave();
    m_analyzeAct->setChecked(m_tsWasAnalyzing);
    positionChanged();
}

void MainWindow::openTsCollection()
{
    const QString path = QFileDialog::getOpenFileName(this, T("打开题库"), QString(),
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
    m_tsProblems = list;
    m_tsKey = QFileInfo(path).absoluteFilePath();
    m_tsName = QFileInfo(path).completeBaseName();
    if (!m_tsMode) { enterTsumego(); return; }
    QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
    loadTsProblem(st.value(tsProgressGroup() + "/last", 0).toInt());
}

void MainWindow::loadTsProblem(int i)
{
    if (m_tsProblems.isEmpty()) return;
    m_tsIndex = qBound(0, i, int(m_tsProblems.size()) - 1);
    const TsumegoProblem &p = m_tsProblems[m_tsIndex];
    if (!m_tsBestId.isEmpty()) m_engine->terminate(m_tsBestId);
    if (!m_tsReplyId.isEmpty()) m_engine->terminate(m_tsReplyId);
    m_tsBestId.clear();
    m_tsReplyId.clear();
    Sgf::load(Tsumego::toSgf(p), m_tree, nullptr);
    m_tsPlayer = p.toPlay;
    // 判题和允许落子的范围：棋子外扩 1 路；显示范围再大一点
    m_tsRect = Tsumego::region(p, 1);
    m_tsAllow = QJsonArray();
    const int n = m_tree.boardSize();
    for (int y = m_tsRect.top(); y <= m_tsRect.bottom(); ++y)
        for (int x = m_tsRect.left(); x <= m_tsRect.right(); ++x)
            m_tsAllow.append(BoardWidget::toGtp(x, y, n));
    m_tsAllow.append(QStringLiteral("pass"));
    m_board->setViewRect(Tsumego::region(p, 2));
    m_board->setPinnedPv({});
    m_board->setMoveMarks({});
    QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
    st.setValue(tsProgressGroup() + "/last", m_tsIndex);
    m_tsMessage = T("轮到你：%1").arg(Tsumego::goalText(p));
    tsSendBest();
}

QJsonObject MainWindow::tsQuery(Stone player) const
{
    QJsonObject q = buildQuery();
    q.remove("reportDuringSearchEvery");
    // 显卡版每步多算一些，CPU 版少一些免得等太久
    q["maxVisits"] = m_settings.katagoPath.contains("cpu", Qt::CaseInsensitive) ? 300 : 800;
    q["allowMoves"] = QJsonArray{QJsonObject{{"player", colorStr(player)}, {"moves", m_tsAllow}, {"untilDepth", 1}}};
    q["overrideSettings"] = QJsonObject{{"wideRootNoise", 0.0}};
    q["includeOwnership"] = true;
    q["includePolicy"] = false;
    return q;
}

void MainWindow::tsSendBest()
{
    m_tsState = TsState::Thinking;
    m_tsBestOwn = QJsonArray();
    m_tsBestPv.clear();
    tsRefresh();
    if (m_engine->state() != KataGoEngine::State::Ready) return; // 引擎就绪时会再调用
    QJsonObject q = tsQuery(m_tree.toPlay());
    q["includeMovesOwnership"] = true;
    m_tsBestId = m_engine->analyze(q);
}

void MainWindow::tsOnClick(int x, int y)
{
    if (m_tsState == TsState::Thinking || m_tsState == TsState::Replying) {
        statusBar()->showMessage(T("请稍等，正在看棋…"), 2000);
        return;
    }
    if (m_tsState == TsState::Solved || m_tsState == TsState::Failed) {
        statusBar()->showMessage(T("这道题已经结束：点“重来”再做一次，或“下一题”"), 3000);
        return;
    }
    if (!m_tsRect.contains(x, y)) {
        statusBar()->showMessage(T("请在题目范围内落子"), 3000);
        QApplication::beep();
        return;
    }
    if (!m_tree.play(x, y)) { QApplication::beep(); return; }
    m_tsUserMove = BoardWidget::toGtp(x, y, m_tree.boardSize());
    m_board->setPinnedPv({});
    m_tsState = TsState::Replying;
    m_board->update();
    tsRefresh();
    m_tsReplyId = m_engine->analyze(tsQuery(m_tree.toPlay()));
}

bool MainWindow::tsHandleResponse(const QJsonObject &resp)
{
    const QString id = resp.value("id").toString();
    if (id != m_tsBestId && id != m_tsReplyId) return false;
    if (resp.value("isDuringSearch").toBool()) return true;
    const QJsonArray infos = resp.value("moveInfos").toArray();
    const int n = m_tree.boardSize();

    if (id == m_tsBestId) {
        m_tsBestId.clear();
        if (infos.isEmpty()) return true;
        const QJsonObject best = infos.first().toObject();
        m_tsBestMove = best.value("move").toString();
        m_tsBestOwn = best.value("ownership").toArray();
        m_tsBestPv.clear();
        // 参考答案只显示题目范围内的部分（后面脱先去别处的不显示）
        for (const auto &v : best.value("pv").toArray()) {
            int px, py;
            if (!BoardWidget::fromGtp(v.toString(), n, &px, &py) || !m_tsRect.contains(px, py) || m_tsBestPv.size() >= 12) break;
            m_tsBestPv << v.toString();
        }
        m_tsState = TsState::UserTurn;
        tsRefresh();
        // 开发用：NANOGO_TSUMEGO_MOVE=Q19 自动替你下这一手（自动测试用，只用一次）
        static QString autoMove = qEnvironmentVariable("NANOGO_TSUMEGO_MOVE");
        if (!autoMove.isEmpty()) {
            int x, y;
            const QString mv = autoMove;
            autoMove.clear();
            if (mv == "answer") tsShowAnswer();
            else if (BoardWidget::fromGtp(mv, n, &x, &y)) tsOnClick(x, y);
        }
        return true;
    }

    // 对方的应手：比较归属
    m_tsReplyId.clear();
    const QJsonArray own = resp.value("ownership").toArray();
    double diff = 0;
    if (m_tsUserMove.compare(m_tsBestMove, Qt::CaseInsensitive) != 0 && own.size() == n * n && m_tsBestOwn.size() == n * n)
        for (int y = m_tsRect.top(); y <= m_tsRect.bottom(); ++y)
            for (int x = m_tsRect.left(); x <= m_tsRect.right(); ++x)
                diff += qAbs(m_tsBestOwn[y * n + x].toDouble() - own[y * n + x].toDouble());
    const QString reply = infos.isEmpty() ? QStringLiteral("pass") : infos.first().toObject().value("move").toString();
    auto playReply = [&] {
        int rx, ry;
        if (BoardWidget::fromGtp(reply, n, &rx, &ry)) m_tree.play(rx, ry);
    };

    if (diff >= kWrongThreshold) {
        // 下错了：对方的应手在棋子附近就摆出来，让你看到为什么不行；在外面说明对方根本不用理
        int rx = -1, ry = -1;
        const bool local = BoardWidget::fromGtp(reply, n, &rx, &ry)
                           && Tsumego::region(m_tsProblems.value(m_tsIndex), 0).contains(rx, ry);
        if (local) playReply();
        m_tsState = TsState::Failed;
        m_tsMessage = local ? T("不对：对方应 %1 后，你就不行了。").arg(reply)
                            : T("不对，这手不能达到目的（对方可以不理）。");
        m_board->update();
        tsRefresh();
        return true;
    }

    // 下对了：题目范围内的棋子死活都定了（或对方无棋可走），就算做出来了
    bool settled = own.size() == n * n;
    const Board &b = m_tree.board();
    for (int y = m_tsRect.top(); settled && y <= m_tsRect.bottom(); ++y)
        for (int x = m_tsRect.left(); x <= m_tsRect.right(); ++x)
            if (b.at(x, y) != Stone::Empty && qAbs(own[y * n + x].toDouble()) < 0.75) { settled = false; break; }
    int myMoves = 0;
    for (const GameNode *g = m_tree.current(); g && g->move.isValid(); g = g->parent) ++myMoves;
    if (settled || reply.compare("pass", Qt::CaseInsensitive) == 0 || myMoves >= 24) {
        m_tsState = TsState::Solved;
        m_tsMessage = T("正确！");
        QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
        st.setValue(tsProgressGroup() + QStringLiteral("/solved%1").arg(m_tsIndex), true);
        tsRefresh();
        return true;
    }
    playReply();
    m_tsMessage = T("好棋！对方应 %1，继续。").arg(reply);
    m_board->update();
    tsSendBest();
    return true;
}

// 悔棋 / 前进后退之后：退到轮你走，再重新判断
void MainWindow::tsAfterNavigate()
{
    if (!m_tsBestId.isEmpty()) m_engine->terminate(m_tsBestId);
    if (!m_tsReplyId.isEmpty()) m_engine->terminate(m_tsReplyId);
    m_tsBestId.clear();
    m_tsReplyId.clear();
    if (m_tree.toPlay() != m_tsPlayer && m_tree.current() != m_tree.root()) m_tree.back();
    m_board->setPinnedPv({});
    m_board->update();
    m_tsMessage = T("轮到你：%1").arg(Tsumego::goalText(m_tsProblems.value(m_tsIndex)));
    tsSendBest();
}

void MainWindow::tsUndo()
{
    if (m_tree.current() == m_tree.root()) return;
    m_tree.back();
    tsAfterNavigate();
}

void MainWindow::tsShowAnswer()
{
    if (m_tsState == TsState::Failed || m_tsState == TsState::Solved) {
        // 退回到下错之前，再显示正解
        if (m_tsState == TsState::Solved) m_tree.toStart();
        else { m_tree.back(); m_tree.back(); }
        m_tsShowAnswerPending = true;
        tsAfterNavigate();
        return;
    }
    if (m_tsState == TsState::UserTurn && m_tsBestPv.isEmpty()) {
        m_tsMessage = T("KataGo 认为这里已经不需要再下了。");
        tsRefresh();
        return;
    }
    if (m_tsState != TsState::UserTurn) {
        m_tsShowAnswerPending = true;
        statusBar()->showMessage(T("正在计算答案…"), 2000);
        return;
    }
    m_board->setPinnedPv(m_tsBestPv);
    m_tsMessage = T("参考答案：按数字顺序下（第 1 手是 %1）。").arg(m_tsBestPv.first());
    tsRefresh();
}

void MainWindow::tsRefresh()
{
    if (!m_tsMode) return;
    if (m_tsShowAnswerPending && m_tsState == TsState::UserTurn) {
        m_tsShowAnswerPending = false;
        tsShowAnswer();
        return;
    }
    const TsumegoProblem p = m_tsProblems.value(m_tsIndex);
    QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
    int solved = 0;
    for (int i = 0; i < m_tsProblems.size(); ++i)
        if (st.value(tsProgressGroup() + QStringLiteral("/solved%1").arg(i)).toBool()) ++solved;
    const bool thisSolved = st.value(tsProgressGroup() + QStringLiteral("/solved%1").arg(m_tsIndex)).toBool();
    m_tsTitle->setText(T("<b>%1</b>　第 %2 / %3 题 · <b>%4</b>　（已做对 %5 题）")
                           .arg(m_tsName.toHtmlEscaped()).arg(m_tsIndex + 1).arg(m_tsProblems.size())
                           .arg(Tsumego::goalText(p)).arg(solved)
                       + (thisSolved ? QStringLiteral(" ✓") : QString()));
    QString color = "#333", msg = m_tsMessage;
    switch (m_tsState) {
    case TsState::Thinking: msg += T("（正在看题…）"); color = "#888"; break;
    case TsState::Replying: msg = T("对方思考中…"); color = "#888"; break;
    case TsState::Solved: color = "#2a7"; break;
    case TsState::Failed: color = "#d33"; break;
    case TsState::UserTurn: break;
    }
    m_tsStatus->setText(QStringLiteral("<span style='font-size:large;color:%1'><b>%2</b></span>").arg(color, msg.toHtmlEscaped()));
    m_gameInfo->setText(T("<b>死活题练习</b><br>%1<br>第 %2 / %3 题").arg(m_tsName.toHtmlEscaped()).arg(m_tsIndex + 1).arg(m_tsProblems.size()));
    m_analysisInfo->setText(T("<b>%1</b><br><span style='color:gray'>做题时不显示 AI 分析。做不出来可以点“看答案”。</span>").arg(Tsumego::goalText(p)));
    m_refList->clear();
    m_lastMoveRef->clear();
}

// 死活题编辑器：编辑当前题库（内置题库另存为新文件），编好可以直接做
void MainWindow::openTsEditor()
{
    TsumegoEditor dlg(m_engine, this);
    if (!m_tsProblems.isEmpty() && m_tsKey != QLatin1String("builtin")) dlg.setCollection(m_tsProblems, m_tsKey);
    connect(&dlg, &TsumegoEditor::practiceRequested, this,
            [this, &dlg](const QList<TsumegoProblem> &list, const QString &path, int index) {
                dlg.reject();
                if (dlg.isVisible()) return; // 用户取消了保存提示
                m_tsProblems = list;
                m_tsKey = path.isEmpty() ? QStringLiteral("editor") : path;
                m_tsName = path.isEmpty() ? T("编辑中的题库") : QFileInfo(path).completeBaseName();
                if (!m_tsMode) enterTsumego();
                loadTsProblem(index);
            });
    dlg.exec();
}
