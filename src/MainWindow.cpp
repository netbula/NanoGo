#include "MainWindow.h"
#include "BoardWidget.h"
#include "SettingsDialog.h"
#include "Sgf.h"
#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_engine(new KataGoEngine(this)), m_updater(new ModelUpdater(this))
{
    m_settings.load();
    m_tree.setKomi(m_settings.komi);
    m_tree.setRules(m_settings.rules);
    buildUi();
    connect(m_engine, &KataGoEngine::stateChanged, this, &MainWindow::onEngineState);
    connect(m_engine, &KataGoEngine::analysisReceived, this, &MainWindow::onAnalysis);
    connect(m_engine, &KataGoEngine::logLine, this, [this](const QString &l) { m_log->appendPlainText(l); });
    positionChanged();
    m_engine->start(m_settings);

    // 模型自动更新
    connect(m_updater, &ModelUpdater::listReady, this, &MainWindow::onModelList);
    connect(m_updater, &ModelUpdater::failed, this, [this](const QString &msg) {
        m_log->appendPlainText(msg);
        if (m_manualCheck) QMessageBox::warning(this, QStringLiteral("模型更新"), msg);
        else statusBar()->showMessage(msg, 8000);
    });
    connect(m_updater, &ModelUpdater::downloadProgress, this, [this](qint64 d, qint64 t) {
        if (t > 0) statusBar()->showMessage(QStringLiteral("正在下载新模型… %1%（%2 / %3 MB）")
                                                .arg(d * 100 / t).arg(d >> 20).arg(t >> 20));
    });
    connect(m_updater, &ModelUpdater::downloadFinished, this, [this](const QString &path) {
        m_settings.modelPath = path;
        m_settings.save();
        statusBar()->showMessage(QStringLiteral("已切换到新模型 %1（第一次使用需要几分钟显卡调优）").arg(QFileInfo(path).fileName()), 15000);
        m_log->appendPlainText(QStringLiteral("—— 切换模型：%1 ——").arg(path));
        restartEngine();
    });
    if (m_settings.autoUpdateModel
        && (!m_settings.lastModelCheck.isValid() || m_settings.lastModelCheck.secsTo(QDateTime::currentDateTime()) > 24 * 3600))
        QTimer::singleShot(3000, this, [this] { checkModel(false); });
    if (qEnvironmentVariableIsSet("NANOGO_SCREENSHOT")) {
        for (QPoint p : {QPoint(15, 3), QPoint(3, 15), QPoint(16, 15), QPoint(2, 3)}) m_tree.play(p.x(), p.y());
        positionChanged();
        m_analyzeAct->setChecked(true);
    }
}

MainWindow::~MainWindow() { m_engine->stop(); }

void MainWindow::closeEvent(QCloseEvent *e)
{
    m_engine->stop();
    e->accept();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("NanoGo 围棋"));
    resize(1280, 860);

    m_board = new BoardWidget(&m_tree, this);
    connect(m_board, &BoardWidget::pointClicked, this, &MainWindow::onPointClicked);

    auto *side = new QWidget(this);
    side->setMinimumWidth(320);
    auto *v = new QVBoxLayout(side);

    auto *gameBox = new QGroupBox(QStringLiteral("对局"), side);
    auto *gl = new QVBoxLayout(gameBox);
    m_gameInfo = new QLabel(gameBox);
    m_gameInfo->setTextFormat(Qt::RichText);
    gl->addWidget(m_gameInfo);
    v->addWidget(gameBox);

    auto *anaBox = new QGroupBox(QStringLiteral("分析"), side);
    auto *al = new QVBoxLayout(anaBox);
    m_analysisInfo = new QLabel(QStringLiteral("按空格键开始分析"), anaBox);
    m_analysisInfo->setTextFormat(Qt::RichText);
    QFont big = m_analysisInfo->font(); big.setPointSize(big.pointSize() + 2);
    m_analysisInfo->setFont(big);
    al->addWidget(m_analysisInfo);
    v->addWidget(anaBox);

    auto *cBox = new QGroupBox(QStringLiteral("注释"), side);
    auto *cl = new QVBoxLayout(cBox);
    m_comment = new QPlainTextEdit(cBox);
    m_comment->setPlaceholderText(QStringLiteral("这一手的注释…"));
    connect(m_comment, &QPlainTextEdit::textChanged, this, [this] { m_tree.current()->comment = m_comment->toPlainText(); });
    cl->addWidget(m_comment);
    v->addWidget(cBox, 1);

    auto *logBox = new QGroupBox(QStringLiteral("引擎日志"), side);
    auto *ll = new QVBoxLayout(logBox);
    m_log = new QPlainTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    QFont mono("Consolas"); mono.setStyleHint(QFont::Monospace); mono.setPointSize(8);
    m_log->setFont(mono);
    ll->addWidget(m_log);
    v->addWidget(logBox, 1);

    auto *split = new QSplitter(this);
    split->addWidget(m_board);
    split->addWidget(side);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    setCentralWidget(split);

    m_engineStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_engineStatus, 1);

    // 菜单
    QMenu *file = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    file->addAction(QStringLiteral("新对局（摆谱 / 分析）"), QKeySequence::New, this, &MainWindow::newGame);
    file->addAction(QStringLiteral("人机对弈…"), QKeySequence(Qt::CTRL | Qt::Key_G), this, &MainWindow::newAiGame);
    file->addAction(QStringLiteral("打开棋谱…"), QKeySequence::Open, this, &MainWindow::openSgf);
    file->addAction(QStringLiteral("保存棋谱…"), QKeySequence::Save, this, &MainWindow::saveSgf);
    file->addSeparator();
    file->addAction(QStringLiteral("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *nav = menuBar()->addMenu(QStringLiteral("棋局(&G)"));
    auto addNav = [&](const QString &t, QKeySequence k, auto fn) {
        QAction *a = nav->addAction(t, k, this, [this, fn] { if ((m_tree.*fn)()) positionChanged(); });
        return a;
    };
    QAction *backA = addNav(QStringLiteral("后退一步"), QKeySequence(Qt::Key_Left), &GameTree::back);
    QAction *fwdA = addNav(QStringLiteral("前进一步"), QKeySequence(Qt::Key_Right), &GameTree::forward);
    QAction *startA = nav->addAction(QStringLiteral("回到开头"), QKeySequence(Qt::Key_Home), this, [this] { m_tree.toStart(); positionChanged(); });
    QAction *endA = nav->addAction(QStringLiteral("走到最后"), QKeySequence(Qt::Key_End), this, [this] { m_tree.toEnd(); positionChanged(); });
    nav->addSeparator();
    QAction *passA = nav->addAction(QStringLiteral("停一手"), QKeySequence(Qt::CTRL | Qt::Key_P), this, [this] { m_tree.pass(); positionChanged(); });

    QMenu *ana = menuBar()->addMenu(QStringLiteral("分析(&A)"));
    m_analyzeAct = ana->addAction(QStringLiteral("开始/停止分析"));
    m_analyzeAct->setShortcut(QKeySequence(Qt::Key_Space));
    m_analyzeAct->setCheckable(true);
    connect(m_analyzeAct, &QAction::toggled, this, [this](bool on) {
        if (on) requestAnalysis();
        else { m_engine->terminate(m_queryId); m_queryId.clear(); }
        refreshInfo();
    });
    m_candAct = ana->addAction(QStringLiteral("显示候选点"));
    m_candAct->setCheckable(true);
    m_candAct->setChecked(true);
    m_candAct->setShortcut(QKeySequence(Qt::Key_H));
    connect(m_candAct, &QAction::toggled, m_board, &BoardWidget::setShowCandidates);
    m_ownAct = ana->addAction(QStringLiteral("显示形势（地盘）"));
    m_ownAct->setCheckable(true);
    m_ownAct->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_ownAct, &QAction::toggled, m_board, &BoardWidget::setShowOwnership);
    ana->addSeparator();
    ana->addAction(QStringLiteral("重启引擎"), this, &MainWindow::restartEngine);

    QMenu *set = menuBar()->addMenu(QStringLiteral("设置(&S)"));
    set->addAction(QStringLiteral("引擎与对局设置…"), this, &MainWindow::openSettings);
    set->addSeparator();
    set->addAction(QStringLiteral("检查模型更新…"), this, [this] { checkModel(true); });
    QAction *autoUpd = set->addAction(QStringLiteral("自动切换到官方最强模型（每天检查一次）"));
    autoUpd->setCheckable(true);
    autoUpd->setChecked(m_settings.autoUpdateModel);
    connect(autoUpd, &QAction::toggled, this, [this](bool on) { m_settings.autoUpdateModel = on; m_settings.save(); });

    QToolBar *tb = addToolBar(QStringLiteral("工具栏"));
    tb->setMovable(false);
    tb->addAction(QStringLiteral("人机对弈"), this, &MainWindow::newAiGame);
    tb->addSeparator();
    tb->addAction(startA);
    tb->addAction(backA);
    tb->addAction(fwdA);
    tb->addAction(endA);
    tb->addSeparator();
    tb->addAction(passA);
    tb->addSeparator();
    tb->addAction(m_analyzeAct);
    tb->addAction(m_candAct);
    tb->addAction(m_ownAct);
}

void MainWindow::newGame()
{
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_aiNode = nullptr;
    m_engine->terminateAll();
    m_queryId.clear();
    m_queryNode = nullptr;
    m_tree.adoptRoot(new GameNode, 19);
    m_tree.setKomi(m_settings.komi);
    m_tree.setRules(m_settings.rules);
    m_tree.playerBlack.clear(); m_tree.playerWhite.clear(); m_tree.result.clear();
    m_sgfPath.clear();
    positionChanged();
}

void MainWindow::openSgf()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开棋谱"), QFileInfo(m_sgfPath).absolutePath(),
                                                      QStringLiteral("SGF 棋谱 (*.sgf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法读取文件：%1").arg(path));
        return;
    }
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_engine->terminateAll();
    m_queryId.clear();
    m_queryNode = nullptr;
    QString err;
    if (!Sgf::load(Sgf::decode(f.readAll()), m_tree, &err)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), err);
        newGame();
        return;
    }
    m_sgfPath = path;
    setWindowTitle(QStringLiteral("NanoGo 围棋 - %1").arg(QFileInfo(path).fileName()));
    positionChanged();
}

void MainWindow::saveSgf()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存棋谱"), m_sgfPath, QStringLiteral("SGF 棋谱 (*.sgf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".sgf", Qt::CaseInsensitive)) path += ".sgf";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入：%1").arg(path));
        return;
    }
    f.write(Sgf::save(m_tree).toUtf8());
    m_sgfPath = path;
    statusBar()->showMessage(QStringLiteral("已保存到 %1").arg(path), 5000);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(m_settings, m_engine->openclDevices(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    AppSettings old = m_settings;
    m_settings = dlg.result();
    m_settings.save();
    const bool engineChanged = old.katagoPath != m_settings.katagoPath || old.modelPath != m_settings.modelPath
                               || old.gpuDevice != m_settings.gpuDevice || old.searchThreads != m_settings.searchThreads;
    if (engineChanged || m_engine->state() == KataGoEngine::State::Failed) restartEngine();
    else if (m_analyzeAct->isChecked()) requestAnalysis();
}

void MainWindow::restartEngine()
{
    m_queryId.clear();
    m_queryNode = nullptr;
    m_log->appendPlainText(QStringLiteral("—— 重启引擎 ——"));
    m_engine->start(m_settings);
}

void MainWindow::onPointClicked(int x, int y)
{
    if (m_aiColor != Stone::Empty && m_tree.toPlay() == m_aiColor) {
        statusBar()->showMessage(QStringLiteral("AI 正在思考，请稍候…"), 2000);
        return;
    }
    if (m_tree.play(x, y)) positionChanged();
    else QApplication::beep();
}

void MainWindow::onEngineState(KataGoEngine::State s, const QString &msg)
{
    QString color = s == KataGoEngine::State::Ready ? "green" : s == KataGoEngine::State::Failed ? "red" : "#b07000";
    m_engineStatus->setText(QStringLiteral("<span style='color:%1'>●</span> %2").arg(color, msg.section('\n', 0, 0).toHtmlEscaped()));
    if (s == KataGoEngine::State::Ready && m_analyzeAct->isChecked()) requestAnalysis();
    if (s == KataGoEngine::State::Ready) { m_aiQueryId.clear(); maybeAiMove(); }
    if (s == KataGoEngine::State::Failed) {
        QMessageBox box(QMessageBox::Warning, QStringLiteral("引擎出错"), msg, QMessageBox::Ok, this);
        QPushButton *settingsBtn = box.addButton(QStringLiteral("打开设置"), QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == static_cast<QAbstractButton *>(settingsBtn)) openSettings();
    }
}

QJsonObject MainWindow::buildQuery() const
{
    const int n = m_tree.boardSize();
    QJsonObject q;
    q["rules"] = m_tree.rules();
    q["komi"] = m_tree.komi();
    q["boardXSize"] = n;
    q["boardYSize"] = n;
    q["maxVisits"] = m_settings.maxVisits;
    q["includeOwnership"] = true;
    q["includePVVisits"] = false;
    q["reportDuringSearchEvery"] = 0.4;
    q["analyzeTurns"] = QJsonArray();

    const QList<GameNode *> path = m_tree.pathToCurrent();
    bool midSetup = false;
    for (int i = 1; i < path.size(); ++i)
        if (!path[i]->addBlack.isEmpty() || !path[i]->addWhite.isEmpty()) midSetup = true;

    QJsonArray initial, moves;
    auto colorStr = [](Stone s) { return s == Stone::Black ? QStringLiteral("B") : QStringLiteral("W"); };
    if (midSetup) {
        // 中途有摆子：直接把当前盘面当作初始局面
        const Board &b = m_tree.board();
        for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x)
            if (b.at(x, y) != Stone::Empty) initial.append(QJsonArray{colorStr(b.at(x, y)), BoardWidget::toGtp(x, y, n)});
    } else {
        for (const QPoint &p : path[0]->addBlack) initial.append(QJsonArray{"B", BoardWidget::toGtp(p.x(), p.y(), n)});
        for (const QPoint &p : path[0]->addWhite) initial.append(QJsonArray{"W", BoardWidget::toGtp(p.x(), p.y(), n)});
        for (int i = 1; i < path.size(); ++i) {
            const Move &m = path[i]->move;
            if (m.isValid()) moves.append(QJsonArray{colorStr(m.color), BoardWidget::toGtp(m.x, m.y, n)});
        }
    }
    q["initialStones"] = initial;
    q["moves"] = moves;
    if (moves.isEmpty()) q["initialPlayer"] = colorStr(m_tree.toPlay());
    q["analyzeTurns"] = QJsonArray{moves.size()};
    return q;
}

void MainWindow::requestAnalysis()
{
    if (!m_analyzeAct->isChecked() || m_engine->state() != KataGoEngine::State::Ready) return;
    if (!m_queryId.isEmpty()) m_engine->terminate(m_queryId);
    m_queryNode = m_tree.current();
    m_queryId = m_engine->analyze(buildQuery());
}

void MainWindow::onAnalysis(const QJsonObject &resp)
{
    if (!m_aiQueryId.isEmpty() && resp.value("id").toString() == m_aiQueryId) {
        if (resp.value("isDuringSearch").toBool()) return;
        m_aiQueryId.clear();
        if (m_aiColor == Stone::Empty || m_aiNode != m_tree.current() || m_tree.toPlay() != m_aiColor) return;
        const QJsonArray infos = resp.value("moveInfos").toArray();
        QString best;
        if (!infos.isEmpty()) {
            best = infos.first().toObject().value("move").toString();
        } else {
            // 计算量为 1 时 KataGo 不展开候选点，只能用策略网络（直觉）最高的点
            const QJsonArray policy = resp.value("policy").toArray();
            const int n = m_tree.boardSize();
            int bestIdx = -1;
            double bestP = -1;
            for (int i = 0; i < policy.size(); ++i) {
                const double p = policy[i].toDouble();
                if (p > bestP) { bestP = p; bestIdx = i; }
            }
            best = (bestIdx >= 0 && bestIdx < n * n) ? BoardWidget::toGtp(bestIdx % n, bestIdx / n, n) : QStringLiteral("pass");
        }
        // AI 胜率极低时认输（低于 3% 且已过 100 手）
        const double wrBlack = resp.value("rootInfo").toObject().value("winrate").toDouble();
        const double aiWr = m_aiColor == Stone::Black ? wrBlack : 1 - wrBlack;
        if (aiWr < 0.03 && m_tree.current()->depth() > 100) {
            m_tree.result = m_aiColor == Stone::Black ? QStringLiteral("W+R") : QStringLiteral("B+R");
            m_aiColor = Stone::Empty;
            refreshInfo();
            QMessageBox::information(this, QStringLiteral("对局结束"), QStringLiteral("AI 认输，你赢了！"));
            return;
        }
        int x, y;
        if (!(BoardWidget::fromGtp(best, m_tree.boardSize(), &x, &y) && m_tree.play(x, y))) m_tree.pass();
        positionChanged();
        return;
    }
    if (resp.value("id").toString() != m_queryId || !m_queryNode) return;
    const QJsonObject root = resp.value("rootInfo").toObject();
    Analysis &a = m_queryNode->analysis;
    a.valid = true;
    a.winrateBlack = root.value("winrate").toDouble();
    a.scoreLeadBlack = root.value("scoreLead").toDouble();
    a.visits = root.value("visits").toInt();
    a.raw = resp;
    if (m_queryNode == m_tree.current()) {
        m_board->update();
        refreshInfo();
    }
}

void MainWindow::positionChanged()
{
    m_comment->blockSignals(true);
    m_comment->setPlainText(m_tree.current()->comment);
    m_comment->blockSignals(false);
    m_board->update();
    refreshInfo();
    requestAnalysis();

    if (m_aiColor != Stone::Empty) {
        const GameNode *cur = m_tree.current();
        if (cur->move.isPass() && cur->move.isValid() && cur->parent && cur->parent->move.isValid() && cur->parent->move.isPass()) {
            m_aiColor = Stone::Empty;
            const Analysis &a = cur->analysis.valid ? cur->analysis : cur->parent->analysis;
            QString msg = QStringLiteral("双方都停了一手，对局结束。");
            if (a.valid) msg += QStringLiteral("\nAI 估计：%1 领先 %2 目").arg(a.scoreLeadBlack >= 0 ? "黑" : "白").arg(qAbs(a.scoreLeadBlack), 0, 'f', 1);
            QMessageBox::information(this, QStringLiteral("对局结束"), msg);
            return;
        }
        maybeAiMove();
    }
}

void MainWindow::maybeAiMove()
{
    if (m_aiColor == Stone::Empty || m_tree.toPlay() != m_aiColor) return;
    if (m_engine->state() != KataGoEngine::State::Ready) {
        statusBar()->showMessage(QStringLiteral("等引擎就绪后 AI 会自动落子"), 3000);
        return;
    }
    if (!m_aiQueryId.isEmpty() && m_aiNode == m_tree.current()) return; // 已经在算了
    if (!m_aiQueryId.isEmpty()) m_engine->terminate(m_aiQueryId);
    QJsonObject q = buildQuery();
    q["maxVisits"] = m_aiVisits;
    q.remove("reportDuringSearchEvery");
    q["includeOwnership"] = false;
    q["includePolicy"] = true;
    m_aiNode = m_tree.current();
    m_aiQueryId = m_engine->analyze(q);
    statusBar()->showMessage(QStringLiteral("AI 思考中…"), 2000);
}

void MainWindow::newAiGame()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("人机对弈"));
    auto *form = new QFormLayout(&dlg);
    auto *color = new QComboBox(&dlg);
    color->addItem(QStringLiteral("我执黑（先走）"), int(Stone::Black));
    color->addItem(QStringLiteral("我执白"), int(Stone::White));
    form->addRow(QStringLiteral("我的颜色："), color);
    auto *handicap = new QSpinBox(&dlg);
    handicap->setRange(0, 9);
    handicap->setSuffix(QStringLiteral(" 子"));
    handicap->setToolTip(QStringLiteral("让子时黑方先摆子，白方先走，贴目自动改为 0.5"));
    form->addRow(QStringLiteral("让子（黑方摆子）："), handicap);
    auto *strength = new QComboBox(&dlg);
    strength->addItem(QStringLiteral("很快（每步 1 次计算，只凭直觉）"), 1);
    strength->addItem(QStringLiteral("快（每步 20 次）"), 20);
    strength->addItem(QStringLiteral("中（每步 100 次）"), 100);
    strength->addItem(QStringLiteral("强（每步 400 次，约 2 秒）"), 400);
    strength->addItem(QStringLiteral("最强（每步 1600 次，约 8 秒）"), 1600);
    strength->setCurrentIndex(qMax(0, strength->findData(m_aiVisits)));
    form->addRow(QStringLiteral("AI 计算量："), strength);
    auto *note = new QLabel(QStringLiteral("提示：zhizi 模型即使只算 1 次也是职业以上水平。\n按人类段位调整强度会在后续版本加入。"), &dlg);
    note->setStyleSheet("color: gray;");
    form->addRow(note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始"));
    bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    newGame();
    const Stone me = Stone(color->currentData().toInt());
    m_aiVisits = strength->currentData().toInt();
    const int h = handicap->value();
    if (h >= 2) {
        // 19 路标准让子位置
        const QList<QPoint> pts2 = {{15, 3}, {3, 15}};
        QList<QPoint> pts = pts2;
        if (h >= 3) pts.append({15, 15});
        if (h >= 4) pts.append({3, 3});
        if (h == 5 || h == 7 || h == 9) pts.append({9, 9});
        if (h >= 6) { pts.append({3, 9}); pts.append({15, 9}); }
        if (h >= 8) { pts.append({9, 3}); pts.append({9, 15}); }
        m_tree.root()->addBlack = pts;
        m_tree.setKomi(0.5);
        m_tree.root()->extraProps["HA"] = {QString::number(h)};
    }
    m_tree.playerBlack = me == Stone::Black ? QStringLiteral("我") : QStringLiteral("NanoGo AI");
    m_tree.playerWhite = me == Stone::White ? QStringLiteral("我") : QStringLiteral("NanoGo AI");
    m_tree.setCurrent(m_tree.root());
    m_aiColor = opponent(me);
    setWindowTitle(QStringLiteral("NanoGo 围棋 - 人机对弈"));
    positionChanged();
}

void MainWindow::refreshInfo()
{
    const GameNode *cur = m_tree.current();
    const int moveNo = cur->depth();
    const QString toPlay = m_tree.toPlay() == Stone::Black ? QStringLiteral("黑") : QStringLiteral("白");
    const QString pb = m_tree.playerBlack.isEmpty() ? QStringLiteral("黑方") : m_tree.playerBlack.toHtmlEscaped();
    const QString pw = m_tree.playerWhite.isEmpty() ? QStringLiteral("白方") : m_tree.playerWhite.toHtmlEscaped();
    QString lastMove;
    if (cur->move.isValid())
        lastMove = QStringLiteral("，上一手 %1 %2").arg(cur->move.color == Stone::Black ? "黑" : "白",
                                                     BoardWidget::toGtp(cur->move.x, cur->move.y, m_tree.boardSize()));
    m_gameInfo->setText(QStringLiteral("<b>%1</b> 对 <b>%2</b><br>第 %3 手%4<br>轮到 %5 走 · 贴 %6 目<br>提子：黑 %7 · 白 %8%9")
                            .arg(pb, pw).arg(moveNo).arg(lastMove, toPlay).arg(m_tree.komi())
                            .arg(m_tree.board().capturesBlack()).arg(m_tree.board().capturesWhite())
                            .arg(m_tree.result.isEmpty() ? QString() : QStringLiteral("<br>结果：%1").arg(m_tree.result.toHtmlEscaped())));

    const Analysis &a = cur->analysis;
    if (!a.valid) {
        m_analysisInfo->setText(m_analyzeAct->isChecked() ? QStringLiteral("计算中…") : QStringLiteral("按空格键开始分析"));
        return;
    }
    const double lead = a.scoreLeadBlack;
    const QString leader = lead >= 0 ? QStringLiteral("黑") : QStringLiteral("白");
    m_analysisInfo->setText(QStringLiteral("黑胜率 <b>%1%</b><br>%2 领先 <b>%3</b> 目<br><span style='color:gray'>计算量 %4</span>")
                                .arg(a.winrateBlack * 100, 0, 'f', 1).arg(leader).arg(qAbs(lead), 0, 'f', 1).arg(a.visits));
}

void MainWindow::checkModel(bool manual)
{
    if (m_updater->busy()) {
        if (manual) QMessageBox::information(this, QStringLiteral("模型更新"), QStringLiteral("正在检查或下载中，请稍候。"));
        return;
    }
    m_manualCheck = manual;
    statusBar()->showMessage(QStringLiteral("正在从 katagotraining.org 检查最新模型…"), 5000);
    m_updater->checkForUpdates();
}

void MainWindow::onModelList(const QList<NetworkInfo> &list)
{
    m_settings.lastModelCheck = QDateTime::currentDateTime();
    m_settings.save();
    const NetworkInfo best = ModelUpdater::strongest(list);
    if (best.name.isEmpty()) {
        if (m_manualCheck) QMessageBox::warning(this, QStringLiteral("模型更新"), QStringLiteral("没有从官方列表里找到可用的模型。"));
        return;
    }
    const NetworkInfo *cur = ModelUpdater::find(list, m_settings.modelPath);
    auto desc = [](const NetworkInfo &n) {
        return QStringLiteral("%1\n等级分 %2 ± %3（%4 局评测）").arg(n.name).arg(n.elo, 0, 'f', 0).arg(n.eloUncertainty, 0, 'f', 0).arg(n.games);
    };
    m_log->appendPlainText(QStringLiteral("官方最强模型：") + desc(best).replace('\n', ' '));

    if (cur && (cur->name == best.name || cur->eloLower >= best.eloLower)) {
        if (m_manualCheck)
            QMessageBox::information(this, QStringLiteral("模型更新"), QStringLiteral("当前用的已经是最强模型：\n%1").arg(desc(*cur)));
        else statusBar()->showMessage(QStringLiteral("当前模型已是官方最强"), 5000);
        return;
    }

    const QString sizeMb = QString::number(best.bytes >> 20);
    // 当前模型在官方列表里且开了自动更新：直接下载切换；否则先问
    if (!(cur && m_settings.autoUpdateModel)) {
        const QString curText = cur ? desc(*cur) : QStringLiteral("%1（不在官方列表里，无法比较）").arg(QFileInfo(m_settings.modelPath).fileName());
        auto ans = QMessageBox::question(this, QStringLiteral("发现更强的模型"),
            QStringLiteral("官方最强模型：\n%1\n\n当前模型：\n%2\n\n要下载（约 %3 MB）并切换吗？旧模型会保留。")
                .arg(desc(best), curText, sizeMb));
        if (ans != QMessageBox::Yes) return;
    } else {
        m_log->appendPlainText(QStringLiteral("自动更新：%1 → %2（高 %3 分）").arg(cur->name, best.name).arg(best.elo - cur->elo, 0, 'f', 0));
    }
    statusBar()->showMessage(QStringLiteral("开始下载新模型 %1（%2 MB）…").arg(best.name, sizeMb), 5000);
    m_updater->download(best);
}
