#include "I18n.h"
#include "MainWindow.h"
#include "EngineInstaller.h"
#include "Icons.h"
#include "WinrateGraph.h"
#include "TsumegoEditor.h"
#include "ModelChooser.h"
#include "BoardWidget.h"
#include "HardwareInfo.h"
#include "SettingsDialog.h"
#include "Sgf.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QPushButton>
#include <QProcess>
#include <QProgressDialog>
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
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QDir>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QSettings>
#include <QSplitter>
#include <QRandomGenerator>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

// AI 强度用一个整数表示：正数 = 每步计算量；负数 = 模仿人类段位（-k = k 级，-(100+d) = d 段）
static bool isRank(int v) { return v < 0; }
// -(1000+年份) = 某一年的职业棋手（KataGo 人类模型按职业棋谱的年份学习；职业没有按段细分的数据）
static int proYear(int v) { return v <= -1000 ? -v - 1000 : 0; }
static QString rankProfile(int v)
{
    if (proYear(v)) return QStringLiteral("proyear_%1").arg(proYear(v));
    return v <= -100 ? QStringLiteral("rank_%1d").arg(-v - 100) : QStringLiteral("rank_%1k").arg(-v);
}
static QString rankLabel(int v) { return v <= -100 ? T("%1 段").arg(-v - 100) : T("%1 级").arg(-v); }

void fillStrengthBox(QComboBox *box, bool verbose)
{
    for (auto [label, v] : std::initializer_list<std::pair<const char *, int>>{
             {"直觉（1 次）", 1}, {"快（20 次）", 20}, {"中（100 次）", 100}, {"强（400 次）", 400},
             {"很强（1600 次）", 1600}, {"最强（6400 次）", 6400}})
        box->addItem(T(label), v);
    box->insertSeparator(box->count());
    for (int v : {-18, -15, -12, -10, -8, -6, -5, -4, -3, -2, -1, -101, -102, -103, -104, -105, -106, -107, -108, -109}) // 按野狐的段级位：18 级到 9 段
        box->addItem(verbose ? T("像野狐 %1 那样下").arg(rankLabel(v)) : T("野狐 %1").arg(rankLabel(v)), v);
    for (int year : {2023, 2015, 2005, 1995, 1985, 1970, 1950, 1930})
        box->addItem(verbose ? T("像 %1 年的职业棋手那样下").arg(year) : T("职业 %1 年").arg(year), -1000 - year);
}

QString strengthText(int v) { return proYear(v) ? T("职业 %1 年").arg(proYear(v)) : isRank(v) ? T("野狐 %1").arg(rankLabel(v)) : T("每步 %1 次计算").arg(v); }


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_engine(new KataGoEngine(this)), m_updater(new ModelUpdater(this)), m_installer(new EngineInstaller(this))
{
    m_settings.load();
    m_tree.setKomi(m_settings.komi);
    m_tree.setRules(m_settings.rules);
    buildUi();
    connect(m_engine, &KataGoEngine::stateChanged, this, &MainWindow::onEngineState);
    connect(m_engine, &KataGoEngine::analysisReceived, this, &MainWindow::onAnalysis);
    connect(m_engine, &KataGoEngine::tuned, this, [this](int t) {
        m_settings.tunedThreads = t;
        m_settings.tunedFor = m_settings.tuneKey();
        appendLog(T("已记住本机最佳搜索线程数：%1").arg(t));
        m_settings.save();
    });
    connect(m_engine, &KataGoEngine::prepareProgress, this, [this](int pct, const QString &text) {
        if (!m_prepProgress) {
            m_prepProgress = new QProgressDialog(this);
            m_prepProgress->setWindowTitle(T("准备引擎"));
            if (AppSettings::katagoBackend(m_settings.katagoPath) == "opencl") {
                // 允许人工换显卡：按下后选一块，用它重新调优
                m_prepProgress->setCancelButtonText(T("换一块显卡…"));
                connect(m_prepProgress, &QProgressDialog::canceled, this, [this] {
                    if (!chooseGpu() && m_prepProgress) m_prepProgress->show();
                });
            } else {
                m_prepProgress->setCancelButton(nullptr);
            }
            m_prepProgress->setMinimumWidth(480);
            m_prepProgress->setRange(0, 100);
            m_prepProgress->setAutoClose(false);
            m_prepProgress->setAutoReset(false);
            m_prepProgress->show();
        }
        // 调优只在显卡上才有，所以看到调优就一定是显卡版
        const QString device = AppSettings::isCpuBuild(m_settings.katagoPath)
            ? T("使用 CPU 计算（没有可用的显卡）")
            : (m_settings.gpuName.isEmpty() ? T("显卡：自动") : T("显卡：%1").arg(m_settings.gpuName));
        m_prepProgress->setLabelText(device + "\n\n" + text + "\n\n" + T("只在第一次使用、换显卡或换模型时进行，完成后自动开始。"));
        m_prepProgress->setValue(qBound(0, pct, 100));
    });
    connect(m_engine, &KataGoEngine::logLine, this, [this](const QString &l) { appendLog(l); });
    restoreAutosave();
    positionChanged();
    // 第一次使用：没有引擎就先下载 KataGo 引擎，没有模型再下载最强模型，都齐了才启动
    m_firstModel = m_settings.modelPath.isEmpty();
    connect(m_installer, &EngineInstaller::progress, this, [this](qint64 d, qint64 t) {
        if (t <= 0) return;
        const QString text = T("正在下载 KataGo 引擎… %1%（%2 / %3 MB）").arg(d * 100 / t).arg(d >> 20).arg(t >> 20);
        statusBar()->showMessage(text);
        showDownloadProgress(T("正在从 KataGo 官方网站下载围棋引擎，只需要下载一次。") + "\n"
                                 + T("NanoGo 使用 KataGo 原版引擎：https://github.com/lightvector/KataGo"), text, int(d * 100 / t));
    });
    connect(m_installer, &EngineInstaller::installed, this, [this](const QString &exe) {
        if (m_dlProgress) { m_dlProgress->deleteLater(); m_dlProgress = nullptr; }
        appendLog(T("—— KataGo 引擎已安装：%1 ——").arg(exe));
        m_settings.katagoPath = exe;
        m_settings.save();
        bootstrap();
    });
    connect(m_installer, &EngineInstaller::failed, this, [this](const QString &msg) {
        if (m_dlProgress) { m_dlProgress->deleteLater(); m_dlProgress = nullptr; }
        appendLog(msg);
        m_engineStatus->setText(QStringLiteral("<span style='color:red'>●</span> ") + msg.toHtmlEscaped());
        QMessageBox::warning(this, T("第一次使用"),
            msg + "\n\n" + T("没能自动下载 KataGo 引擎。请检查网络后重新打开 NanoGo。"));
    });
    QTimer::singleShot(300, this, &MainWindow::bootstrap);

    // 模型自动更新
    connect(m_updater, &ModelUpdater::listReady, this, &MainWindow::onModelList);
    connect(m_updater, &ModelUpdater::failed, this, [this](const QString &msg) {
        appendLog(msg);
        if (m_dlProgress) { m_dlProgress->deleteLater(); m_dlProgress = nullptr; }
        m_manualModelDownload = false;
        if (m_humanDownload) {
            m_humanDownload = false;
            m_pendingStrength = 0;
            QMessageBox::warning(this, T("按段位下棋"), msg + "\n\n" + T("可以稍后再选一次段位重新下载。"));
            return;
        }
        if (m_choosingModel) {
            // 连不上官方网站：只列出本机已有的模型
            m_choosingModel = false;
            statusBar()->showMessage(msg, 8000);
            openModelChooser({});
            return;
        }
        if (m_firstModel) {
            QMessageBox::warning(this, T("第一次使用"),
                msg + "\n\n" + T("没能自动下载模型。请检查网络后重新打开 NanoGo，或者从 katagotraining.org 手动下载一个 .bin.gz 模型放进 models 文件夹。"));
            return;
        }
        if (m_manualCheck) QMessageBox::warning(this, T("模型更新"), msg);
        else statusBar()->showMessage(msg, 8000);
    });
    connect(m_updater, &ModelUpdater::downloadProgress, this, [this](qint64 d, qint64 t) {
        if (t <= 0) return;
        const QString text = T("正在下载新模型… %1%（%2 / %3 MB）").arg(d * 100 / t).arg(d >> 20).arg(t >> 20);
        statusBar()->showMessage(text);
        if (m_humanDownload)
            showDownloadProgress(T("正在下载 KataGo 官方的人类风格模型（按段位下棋用），只需要下载一次。") + "\n"
                                     + T("模型来源：https://github.com/lightvector/KataGo/releases"),
                                 text, int(d * 100 / t), T("下载模型"));
        else if (m_firstModel)
            showDownloadProgress(T("正在从 KataGo 官方训练站下载最强的神经网络模型，只需要下载一次。") + "\n"
                                     + T("模型来源：https://katagotraining.org/"), text, int(d * 100 / t));
        else if (m_manualModelDownload)
            showDownloadProgress(T("正在下载你选择的模型。") + "\n" + T("模型来源：https://katagotraining.org/"),
                                 text, int(d * 100 / t), T("下载模型"));
    });
    connect(m_updater, &ModelUpdater::downloadFinished, this, [this](const QString &path) {
        if (m_dlProgress) { m_dlProgress->deleteLater(); m_dlProgress = nullptr; }
        if (m_humanDownload) {
            // 人类风格模型不替换主模型：重启引擎把它一起加载，就绪后切到选好的段位
            m_humanDownload = false;
            appendLog(T("—— 人类风格模型已下载：%1 ——").arg(path));
            statusBar()->showMessage(T("人类风格模型已下载，正在重启引擎…"), 8000);
            restartEngine();
            return;
        }
        m_firstModel = false;
        m_manualModelDownload = false;
        m_settings.modelPath = path;
        m_settings.save();
        statusBar()->showMessage(T("已切换到新模型 %1（第一次使用需要几分钟显卡调优）").arg(QFileInfo(path).fileName()), 15000);
        appendLog(T("—— 切换模型：%1 ——").arg(path));
        restartEngine();
    });
    if (!m_firstModel && m_settings.autoUpdateModel
        && (!m_settings.lastModelCheck.isValid() || m_settings.lastModelCheck.secsTo(QDateTime::currentDateTime()) > 24 * 3600))
        QTimer::singleShot(3000, this, [this] { checkModel(false); });
    // 开发用：NANOGO_TSEDITOR=截图文件 打开题目编辑器（载入内置题），检查后截图退出
    if (qEnvironmentVariableIsSet("NANOGO_TSEDITOR")) {
        auto *ed = new TsumegoEditor(m_engine, this);
        ed->setCollection(Tsumego::builtin(), {});
        ed->show();
        connect(m_engine, &KataGoEngine::stateChanged, ed, [ed](KataGoEngine::State st) {
            if (st == KataGoEngine::State::Ready) QMetaObject::invokeMethod(ed, "check", Qt::QueuedConnection);
        });
        QTimer::singleShot(qEnvironmentVariableIntValue("NANOGO_SCREENSHOT_DELAY"), ed, [ed] {
            ed->grab().save(qEnvironmentVariable("NANOGO_TSEDITOR"));
            QApplication::quit();
        });
    } else if (qEnvironmentVariableIsSet("NANOGO_TSUMEGO")) {
        QTimer::singleShot(500, this, [this] {
            enterTsumego();
            loadTsProblem(qEnvironmentVariableIntValue("NANOGO_TSUMEGO") - 1);
        });
    } else if (qEnvironmentVariableIsSet("NANOGO_SCREENSHOT") && !qEnvironmentVariableIsSet("NANOGO_AUTOREVIEW")) {
        for (QPoint p : {QPoint(15, 3), QPoint(3, 15), QPoint(16, 15), QPoint(2, 3)}) m_tree.play(p.x(), p.y());
        positionChanged();
        m_analyzeAct->setChecked(true);
    }
}

MainWindow::~MainWindow() { m_engine->stop(); }

// 引擎日志同时写进 data/nanogo.log，不靠弹窗也能事后查问题；超过 2 MB 换新文件
void MainWindow::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
    const QString path = QDir(AppSettings::dataDir()).filePath("nanogo.log");
    if (QFileInfo(path).size() > 2 * 1024 * 1024) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + line + "\n").toUtf8());
}

void MainWindow::showDownloadProgress(const QString &what, const QString &text, int percent, const QString &title)
{
    if (!m_dlProgress) {
        m_dlProgress = new QProgressDialog(this);
        m_dlProgress->setWindowTitle(title.isEmpty() ? T("第一次使用") : title);
        m_dlProgress->setCancelButton(nullptr);
        m_dlProgress->setMinimumWidth(460);
        m_dlProgress->setRange(0, 100);
        m_dlProgress->show();
    }
    m_dlProgress->setLabelText(what + "\n\n" + text);
    m_dlProgress->setValue(percent);
}

void MainWindow::installEngine(const EngineBuild &b, const QString &why)
{
    if (m_installer->busy() || b.isNull()) return;
    if (!why.isEmpty()) appendLog(why);
    appendLog(T("—— 下载 KataGo %1（%2）——").arg(EngineInstaller::version(), b.kind));
    m_engineStatus->setText(T("正在下载 KataGo 引擎（约 %1 MB），下载完成后自动启动…").arg(b.bytes >> 20));
    m_installer->install(b);
}

// 人工选择显卡：列出本机所有 OpenCL 显卡（型号、显存、集成/独立），选中后用它重新调优并启动
bool MainWindow::chooseGpu()
{
    QString why;
    const QList<GpuInfo> gpus = HardwareInfo::openclGpus(&why);
    if (gpus.isEmpty()) {
        QMessageBox::information(this, T("选择显卡"), why.isEmpty() ? T("没有找到可用的显卡。") : why);
        return false;
    }
    QStringList items{T("自动（推荐：%1）").arg(gpus[HardwareInfo::bestGpu(gpus)].name)};
    int current = 0;
    for (int i = 0; i < gpus.size(); ++i) {
        items << HardwareInfo::describe(gpus[i]);
        if (m_settings.gpuDevice == gpus[i].clIndex) current = i + 1;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, T("选择显卡"), T("用哪块显卡计算？"), items, current, false, &ok);
    if (!ok) return false;
    const int idx = items.indexOf(pick);
    m_settings.gpuDevice = idx <= 0 ? -1 : gpus[idx - 1].clIndex;
    m_gpuOverride = -1;
    m_settings.save();
    appendLog(T("—— 手动选择显卡：%1 ——").arg(pick));
    QTimer::singleShot(0, this, &MainWindow::restartEngine);
    return true;
}

void MainWindow::showAbout()
{
    const QString engine = m_settings.katagoPath.isEmpty() ? T("（尚未下载）") : QFileInfo(m_settings.katagoPath).absolutePath().toHtmlEscaped();
    const QString model = m_settings.modelPath.isEmpty() ? T("（尚未下载）") : QFileInfo(m_settings.modelPath).fileName().toHtmlEscaped();
    QMessageBox box(this);
    box.setWindowTitle(T("关于 NanoGo"));
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    box.setText(T("<h3>NanoGo 围棋</h3>"
                  "<p>NanoGo 是一个围棋复盘、对弈的图形界面，按 MIT 许可证开源：<br>"
                  "<a href='https://github.com/netbula/NanoGo'>https://github.com/netbula/NanoGo</a></p>"
                  "<p><b>NanoGo 的棋力全部来自 KataGo。</b>本程序使用 KataGo 官方原版引擎和官方训练的神经网络模型，"
                  "未做任何修改，只是为它们提供界面。KataGo 由 David J. Wu（lightvector）及贡献者开发，MIT 许可证。</p>"
                  "<p>KataGo 原版：<a href='https://github.com/lightvector/KataGo'>https://github.com/lightvector/KataGo</a><br>"
                  "官方模型与分布式训练：<a href='https://katagotraining.org/'>https://katagotraining.org/</a></p>"
                  "<p>NanoGo 与 KataGo 项目没有隶属关系。</p>"
                  "<p>当前引擎：KataGo %1<br>引擎位置：%2<br>当前模型：%3</p>"
                  "<p>界面使用 Qt %4（LGPL-3.0）：<a href='https://www.qt.io/'>https://www.qt.io/</a></p>")
                    .arg(EngineInstaller::version(), engine, model, QString::fromLatin1(qVersion())));
    box.exec();
}

void MainWindow::bootstrap()
{
    if (m_settings.katagoPath.isEmpty() || !QFileInfo::exists(m_settings.katagoPath)) {
        QString why;
        const EngineBuild b = EngineInstaller::bestForThisPc(&why);
        installEngine(b, why);
        return;
    }
    if (m_settings.modelPath.isEmpty() || !QFileInfo::exists(m_settings.modelPath)) {
        m_firstModel = true;
        m_engineStatus->setText(T("第一次使用：正在下载 KataGo 官方最强模型，下载完成后自动启动…"));
        checkModel(false);
        return;
    }
    startEngine();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    autosave();
    m_engine->stop();
    e->accept();
}

void MainWindow::buildUi()
{
    setWindowTitle(T("NanoGo 围棋"));
    resize(1280, 860);

    m_board = new BoardWidget(&m_tree, this);
    connect(m_board, &BoardWidget::pointClicked, this, &MainWindow::onPointClicked);

    auto *side = new QWidget(this);
    side->setMinimumWidth(320);
    auto *v = new QVBoxLayout(side);

    auto *gameBox = new QGroupBox(T("对局"), side);
    auto *gl = new QVBoxLayout(gameBox);
    m_gameInfo = new QLabel(gameBox);
    m_gameInfo->setTextFormat(Qt::RichText);
    gl->addWidget(m_gameInfo);
    v->addWidget(gameBox);

    auto *anaBox = new QGroupBox(T("分析"), side);
    auto *al = new QVBoxLayout(anaBox);
    m_analysisInfo = new QLabel(T("按空格键开始分析"), anaBox);
    m_analysisInfo->setTextFormat(Qt::RichText);
    m_analysisInfo->setWordWrap(true);
    QFont big = m_analysisInfo->font(); big.setPointSize(big.pointSize() + 2);
    m_analysisInfo->setFont(big);
    al->addWidget(m_analysisInfo);
    v->addWidget(anaBox);

    auto *refBox = new QGroupBox(T("参考图（AI 推荐）"), side);
    auto *rl = new QVBoxLayout(refBox);
    m_lastMoveRef = new QLabel(refBox);
    m_lastMoveRef->setWordWrap(true);
    m_lastMoveRef->setTextFormat(Qt::RichText);
    rl->addWidget(m_lastMoveRef);
    m_refList = new QListWidget(refBox);
    m_refList->setWordWrap(true);
    m_refList->setToolTip(T("点一下在棋盘上显示这条变化，再点一下取消"));
    rl->addWidget(m_refList);
    connect(m_refList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        const QString move = it->data(Qt::UserRole).toString();
        if (move == "__compute__") { sendSideQuery(0, 400); it->setText(T("计算中…")); return; }
        if (move.isEmpty()) return;
        if (m_pinnedMove == move) {
            m_pinnedMove.clear();
            m_refList->clearSelection();
            m_board->setPinnedPv({});
        } else {
            m_pinnedMove = move;
            m_board->setPinnedPv(it->data(Qt::UserRole + 1).toStringList());
        }
    });
    v->addWidget(refBox, 2);

    auto *cBox = new QGroupBox(T("注释"), side);
    auto *cl = new QVBoxLayout(cBox);
    m_comment = new QPlainTextEdit(cBox);
    m_comment->setPlaceholderText(T("这一手的注释…"));
    connect(m_comment, &QPlainTextEdit::textChanged, this, [this] { m_tree.current()->comment = m_comment->toPlainText(); });
    cl->addWidget(m_comment);
    v->addWidget(cBox, 1);

    auto *logBox = new QGroupBox(T("引擎日志"), side);
    auto *ll = new QVBoxLayout(logBox);
    m_log = new QPlainTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    QFont mono("Consolas"); mono.setStyleHint(QFont::Monospace); mono.setPointSize(8);
    m_log->setFont(mono);
    ll->addWidget(m_log);
    v->addWidget(logBox, 1);

    // 棋盘下方：胜率曲线 + 复盘按钮
    auto *reviewPanel = new QWidget(this);
    auto *rv = new QVBoxLayout(reviewPanel);
    rv->setContentsMargins(4, 0, 4, 0);
    auto *rh = new QHBoxLayout;
    m_reviewBtn = new QPushButton(T("分析整盘棋"), reviewPanel);
    m_reviewBtn->setToolTip(T("计算每一手的胜率和目差，画出胜率曲线并标出坏棋"));
    connect(m_reviewBtn, &QPushButton::clicked, this, &MainWindow::startReview);
    rh->addWidget(m_reviewBtn);
    m_reviewVisits = new QComboBox(reviewPanel);
    for (int v : {50, 100, 200, 500, 1000}) m_reviewVisits->addItem(T("每手 %1 次").arg(v), v);
    m_reviewVisits->setCurrentIndex(1);
    m_reviewVisits->setToolTip(T("整盘分析时每一手的计算量，越大越准、越慢"));
    rh->addWidget(m_reviewVisits);
    auto *prevBad = new QPushButton(T("◀ 上一个坏棋"), reviewPanel);
    auto *nextBad = new QPushButton(T("下一个坏棋 ▶"), reviewPanel);
    prevBad->setToolTip(T("跳到上一个恶手或疑问手（PgUp）"));
    nextBad->setToolTip(T("跳到下一个恶手或疑问手（PgDn）"));
    connect(prevBad, &QPushButton::clicked, this, [this] { jumpToMistake(-1); });
    connect(nextBad, &QPushButton::clicked, this, [this] { jumpToMistake(1); });
    rh->addWidget(prevBad);
    rh->addWidget(nextBad);
    m_reviewInfo = new QLabel(reviewPanel);
    m_reviewInfo->setTextFormat(Qt::RichText);
    rh->addWidget(m_reviewInfo, 1);
    rv->addLayout(rh);
    m_graph = new WinrateGraph(reviewPanel);
    connect(m_graph, &WinrateGraph::nodeClicked, this, [this](GameNode *n) {
        if (n == m_tree.current()) return;
        m_aiPaused = m_aiColor != Stone::Empty;
        m_tree.setCurrent(n);
        positionChanged();
    });
    m_graph->setMinimumHeight(80);  // 胜率图至少留出能看清的高度
    rv->addWidget(m_graph, 1);
    m_reviewPanel = reviewPanel;

    // 死活题练习时，棋盘下方换成做题面板
    m_tsPanel = new QWidget(this);
    auto *tv = new QVBoxLayout(m_tsPanel);
    tv->setContentsMargins(4, 0, 4, 0);
    m_tsTitle = new QLabel(m_tsPanel);
    m_tsTitle->setTextFormat(Qt::RichText);
    tv->addWidget(m_tsTitle);
    m_tsStatus = new QLabel(m_tsPanel);
    m_tsStatus->setTextFormat(Qt::RichText);
    m_tsStatus->setWordWrap(true);
    tv->addWidget(m_tsStatus);
    auto *th = new QHBoxLayout;
    auto addTsBtn = [&](const QString &text, auto fn) {
        auto *b = new QPushButton(text, m_tsPanel);
        connect(b, &QPushButton::clicked, this, fn);
        th->addWidget(b);
        return b;
    };
    addTsBtn(T("◀ 上一题"), [this] { loadTsProblem(m_tsIndex - 1); });
    addTsBtn(T("下一题 ▶"), [this] { loadTsProblem(m_tsIndex + 1); });
    addTsBtn(T("重来"), [this] { loadTsProblem(m_tsIndex); });
    addTsBtn(T("悔一步"), [this] { tsUndo(); });
    addTsBtn(T("看答案"), [this] { tsShowAnswer(); });
    th->addStretch(1);
    addTsBtn(T("打开题库…"), [this] { openTsCollection(); })->setToolTip(T("打开 SGF 格式的死活题文件（一个文件里可以有很多题），对错由 KataGo 判断"));
    addTsBtn(T("退出做题"), [this] { exitTsumego(); });
    tv->addLayout(th);
    tv->addStretch(1);
    m_tsPanel->hide();

    auto *left = new QSplitter(Qt::Vertical, this);
    left->addWidget(m_board);
    left->addWidget(reviewPanel);
    left->addWidget(m_tsPanel);
    m_leftSplit = left;
    // 复盘面板和做题面板不能被拖到 0 高度，免得胜率图“消失”
    left->setCollapsible(1, false);
    left->setCollapsible(2, false);
    left->setStretchFactor(0, 5);
    left->setStretchFactor(1, 1);
    left->setSizes({700, 170});

    auto *split = new QSplitter(this);
    split->addWidget(left);
    split->addWidget(side);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    setCentralWidget(split);

    m_engineStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_engineStatus, 1);

    // 菜单
    QMenu *file = menuBar()->addMenu(T("文件(&F)"));
    file->addAction(T("新对局（摆谱 / 分析）"), QKeySequence::New, this, &MainWindow::newGame);
    file->addAction(Icons::aiGame(), T("人机对弈…"), QKeySequence(Qt::CTRL | Qt::Key_G), this, &MainWindow::newAiGame);
    file->addAction(T("打开棋谱…"), QKeySequence::Open, this, &MainWindow::openSgf);
    file->addAction(T("保存棋谱…"), QKeySequence::Save, this, &MainWindow::saveSgf);
    file->addSeparator();
    file->addAction(T("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *nav = menuBar()->addMenu(T("棋局(&G)"));
    // 浏览棋局时 AI 暂停，免得一退回去 AI 又马上把棋下回来
    auto navigate = [this](auto step) {
        m_aiPaused = true;
        if (!m_aiQueryId.isEmpty()) { m_engine->terminate(m_aiQueryId); m_aiQueryId.clear(); }
        if (step()) positionChanged();
    };
    QAction *backA = nav->addAction(T("后退一步（悔棋）"), QKeySequence(Qt::Key_Left), this, [this, navigate] {
        navigate([this] {
            if (!m_tree.back()) return false;
            // 人机对弈：退到轮你走为止（一次退两步）
            if (m_aiColor != Stone::Empty && m_tree.toPlay() == m_aiColor) m_tree.back();
            return true;
        });
    });
    QAction *fwdA = nav->addAction(T("前进一步"), QKeySequence(Qt::Key_Right), this, [this, navigate] {
        navigate([this] { return m_tree.forward(); });
    });
    QAction *startA = nav->addAction(T("回到开头"), QKeySequence(Qt::Key_Home), this, [this, navigate] {
        navigate([this] { m_tree.toStart(); return true; });
    });
    QAction *endA = nav->addAction(T("走到最后"), QKeySequence(Qt::Key_End), this, [this, navigate] {
        navigate([this] { m_tree.toEnd(); return true; });
    });
    nav->addSeparator();
    QAction *passA = nav->addAction(T("停一手"), QKeySequence(Qt::CTRL | Qt::Key_P), this, [this] {
        if (m_aiColor != Stone::Empty && m_tree.toPlay() == m_aiColor) return;
        m_aiPaused = false;
        m_tree.pass();
        positionChanged();
    });
    QAction *aiGoA = nav->addAction(T("让 AI 走这一步"), QKeySequence(Qt::CTRL | Qt::Key_Return), this, [this] {
        if (m_engine->state() != KataGoEngine::State::Ready) {
            statusBar()->showMessage(T("引擎还没就绪，请稍候"), 3000);
            return;
        }
        // 不管轮到谁，都让 AI 替当前一方走一步
        m_aiPaused = false;
        m_aiOneShot = true;
        sendAiQuery();
    });
    aiGoA->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)});
    QAction *swapA = nav->addAction(T("交换黑白"), QKeySequence(Qt::CTRL | Qt::Key_T), this, &MainWindow::swapSides);
    swapA->setToolTip(T("随时交换你和 AI 执的颜色；不在人机对弈中时，从当前局面开始和 AI 下，你走轮到的一方"));
    nav->addSeparator();
    m_tsAct = nav->addAction(T("死活题练习"), QKeySequence(Qt::CTRL | Qt::Key_L));
    m_tsAct->setCheckable(true);
    m_tsAct->setIcon(Icons::tsumego());
    m_tsAct->setIconText(T("死活题"));
    m_tsAct->setToolTip(T("死活题练习：KataGo 判断对错") + " (Ctrl+L)");
    connect(m_tsAct, &QAction::triggered, this, [this](bool on) { on ? enterTsumego() : exitTsumego(); });
    nav->addAction(T("打开死活题库…"), this, &MainWindow::openTsCollection);
    nav->addAction(T("死活题编辑器…"), this, &MainWindow::openTsEditor);

    QMenu *ana = menuBar()->addMenu(T("分析(&A)"));
    m_analyzeAct = ana->addAction(T("开始/停止分析"));
    m_analyzeAct->setShortcut(QKeySequence(Qt::Key_Space));
    m_analyzeAct->setCheckable(true);
    connect(m_analyzeAct, &QAction::toggled, this, [this](bool on) {
        if (on) requestAnalysis();
        else { m_engine->terminate(m_queryId); m_queryId.clear(); }
        refreshInfo();
    });
    m_candAct = ana->addAction(T("显示候选点"));
    m_candAct->setCheckable(true);
    m_candAct->setChecked(true);
    m_candAct->setShortcut(QKeySequence(Qt::Key_H));
    connect(m_candAct, &QAction::toggled, m_board, &BoardWidget::setShowCandidates);
    m_ownAct = ana->addAction(T("显示形势（地盘）"));
    m_ownAct->setCheckable(true);
    m_ownAct->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_ownAct, &QAction::toggled, this, [this](bool on) {
        m_board->setShowOwnership(on);
        // 没开分析时，单独算一次形势
        if (on && !m_analyzeAct->isChecked() && !m_tree.current()->analysis.raw.contains("ownership")) sendSideQuery(0, 200);
    });
    ana->addSeparator();
    QAction *refA = ana->addAction(T("参考图（AI 推荐的下法）"), QKeySequence(Qt::Key_V), this, [this] {
        if (!m_tree.current()->analysis.raw.contains("moveInfos")) sendSideQuery(0, 400);
        else if (m_refList->count() > 0) m_refList->itemClicked(m_refList->item(0)), m_refList->setCurrentRow(0);
    });
    QAction *countA = ana->addAction(T("点目 / 结束对局"), QKeySequence(Qt::CTRL | Qt::Key_E), this, &MainWindow::countScore);
    ana->addSeparator();
    ana->addAction(T("分析整盘棋（胜率曲线、坏棋）"), QKeySequence(Qt::CTRL | Qt::Key_R), this, &MainWindow::startReview);
    ana->addAction(T("上一个坏棋"), QKeySequence(Qt::Key_PageUp), this, [this] { jumpToMistake(-1); });
    ana->addAction(T("下一个坏棋"), QKeySequence(Qt::Key_PageDown), this, [this] { jumpToMistake(1); });
    ana->addSeparator();
    ana->addAction(T("重启引擎"), this, &MainWindow::restartEngine);

    QMenu *set = menuBar()->addMenu(T("设置(&S)"));
    set->addAction(T("引擎与对局设置…"), this, &MainWindow::openSettings);
    set->addSeparator();
    set->addAction(T("选择模型…"), this, &MainWindow::chooseModel);
    set->addAction(T("检查模型更新…"), this, [this] { checkModel(true); });
    QMenu *langMenu = set->addMenu(T("界面语言") + " / Language");
    auto *langGroup = new QActionGroup(this);
    const QList<QPair<QString, QString>> langs = {{"auto", T("自动 / Auto")}, {"zh", QString::fromUtf8("中文")},
                                                  {"en", "English"}, {"ja", QString::fromUtf8("日本語")}, {"ko", QString::fromUtf8("한국어")}};
    for (const auto &l : langs) {
        QAction *a = langMenu->addAction(l.second);
        a->setCheckable(true);
        a->setChecked(m_settings.language == l.first);
        langGroup->addAction(a);
        const QString code = l.first;
        connect(a, &QAction::triggered, this, [this, code] {
            const QString before = m_settings.effectiveLanguage();
            m_settings.language = code;
            m_settings.save();
            if (before == m_settings.effectiveLanguage()) return;
            if (QMessageBox::question(this, T("界面语言"), T("切换语言需要重新打开 NanoGo，现在重新打开吗？（当前棋局会自动保存）"))
                == QMessageBox::Yes) {
                autosave();
                m_engine->stop();
                QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
                QApplication::quit();
            }
        });
    }
    QAction *autoUpd = set->addAction(T("自动切换到官方最强模型（每天检查一次）"));
    m_autoUpdAct = autoUpd;
    autoUpd->setCheckable(true);
    autoUpd->setChecked(m_settings.autoUpdateModel);
    connect(autoUpd, &QAction::toggled, this, [this](bool on) { m_settings.autoUpdateModel = on; m_settings.save(); });

    buildOnlineMenu();

    QMenu *help = menuBar()->addMenu(T("帮助(&H)"));
    help->addAction(T("关于 NanoGo…"), this, &MainWindow::showAbout);
    help->addAction(T("打开日志文件夹"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppSettings::dataDir()));
    });

    // 按钮图示（全部自己画的，见 Icons.cpp）
    startA->setIcon(Icons::first());
    backA->setIcon(Icons::back());
    fwdA->setIcon(Icons::forward());
    endA->setIcon(Icons::last());
    passA->setIcon(Icons::pass());
    aiGoA->setIcon(Icons::aiMove());
    swapA->setIcon(Icons::swap());
    m_analyzeAct->setIcon(Icons::analyze());
    m_candAct->setIcon(Icons::candidates());
    m_ownAct->setIcon(Icons::territory());
    refA->setIcon(Icons::reference());
    countA->setIcon(Icons::count());
    // 工具栏上用短名字，菜单里保留完整名字
    const QList<QPair<QAction *, const char *>> shortNames = {
        {startA, "开头"}, {backA, "后退"}, {fwdA, "前进"}, {endA, "最后"}, {passA, "停一手"},
        {aiGoA, "AI 走一步"}, {swapA, "交换黑白"}, {m_analyzeAct, "分析"}, {m_candAct, "候选点"},
        {m_ownAct, "形势"}, {refA, "参考图"}, {countA, "点目"}};
    for (const auto &sn : shortNames) {
        sn.first->setIconText(T(sn.second));
        if (!sn.first->shortcut().isEmpty())
            sn.first->setToolTip(sn.first->text().remove('&') + " (" + sn.first->shortcut().toString(QKeySequence::NativeText) + ")");
    }

    QToolBar *tb = addToolBar(T("工具栏"));
    tb->setMovable(false);
    tb->setIconSize(QSize(28, 28));
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    QAction *aiGameA = tb->addAction(Icons::aiGame(), T("人机对弈"), this, &MainWindow::newAiGame);
    aiGameA->setToolTip(T("人机对弈") + " (Ctrl+G)");
    tb->addSeparator();
    tb->addAction(startA);
    tb->addAction(backA);
    tb->addAction(fwdA);
    tb->addAction(endA);
    tb->addSeparator();
    tb->addAction(passA);
    tb->addAction(aiGoA);
    tb->addAction(swapA);
    // AI 思考强度：对局中随时可调，下一步立即生效
    tb->addWidget(new QLabel(T(" AI 强度："), tb));
    m_strengthBox = new QComboBox(tb);
    fillStrengthBox(m_strengthBox, false);
    m_strengthBox->setToolTip(T("AI 每步的计算量越大越强；选“野狐 某段/级”时 AI 模仿该水平的人类下法。对局中随时可以改"));
    connect(m_strengthBox, &QComboBox::activated, this, [this] { setAiVisits(m_strengthBox->currentData().toInt()); });
    m_strengthBox->setCurrentIndex(qMax(0, m_strengthBox->findData(m_aiVisits)));
    tb->addWidget(m_strengthBox);
    tb->addSeparator();
    tb->addAction(m_analyzeAct);
    tb->addAction(m_candAct);
    tb->addAction(m_ownAct);
    tb->addAction(refA);
    tb->addSeparator();
    tb->addAction(countA);
    tb->addAction(m_tsAct);
    addOnlineToolbar(tb);
}

// 按段位下棋需要 KataGo 的人类风格模型：没有就问要不要下载，已下载但引擎没加载就重启引擎
bool MainWindow::ensureHumanModel(int v)
{
    if (QFileInfo::exists(AppSettings::humanModelPath())) {
        if (!m_engine->hasHumanModel() && m_engine->state() != KataGoEngine::State::Starting) {
            m_pendingStrength = v;
            restartEngine();
        }
        return true;
    }
    if (m_updater->busy()) {
        QMessageBox::information(this, T("按段位下棋"), T("正在下载别的模型，请等它下完再试。"));
        return false;
    }
    const auto ans = QMessageBox::question(this, T("按段位下棋"),
        T("按人类段位下棋要用 KataGo 官方的“人类风格”模型（%1，约 %2 MB），它学的是各个段位的人类棋谱，下法更像真人。\n\n现在下载吗？只需下载一次。")
            .arg(QStringLiteral(HUMAN_MODEL_FILE)).arg(AppSettings::humanModelBytes >> 20));
    if (ans != QMessageBox::Yes) return false;
    NetworkInfo n;
    n.name = QStringLiteral("b18c384nbt-humanv0");
    n.url = QString::fromLatin1(AppSettings::humanModelUrl);
    n.sha256 = QString::fromLatin1(AppSettings::humanModelSha256);
    n.bytes = AppSettings::humanModelBytes;
    m_humanDownload = true;
    m_pendingStrength = v;
    m_updater->download(n);
    return false;
}

void MainWindow::setAiVisits(int v)
{
    if (isRank(v) && !ensureHumanModel(v)) {
        // 还没有模型（正在下载或用户不下载）：强度框退回原来的选项
        if (m_strengthBox) m_strengthBox->setCurrentIndex(qMax(0, m_strengthBox->findData(m_aiVisits)));
        return;
    }
    if (m_strengthBox) {
        int i = m_strengthBox->findData(v);
        if (i < 0) { m_strengthBox->addItem(isRank(v) ? strengthText(v) : T("%1 次").arg(v), v); i = m_strengthBox->count() - 1; }
        m_strengthBox->setCurrentIndex(i);
    }
    if (v == m_aiVisits) return;
    m_aiVisits = v;
    statusBar()->showMessage(T("AI 强度改为：%1").arg(strengthText(v)), 4000);
    autosave();
    // AI 正在想这一步：按新强度重新想
    if (!m_aiQueryId.isEmpty()) {
        const bool oneShot = m_aiOneShot;
        m_engine->terminate(m_aiQueryId);
        m_aiQueryId.clear();
        m_aiNode = nullptr;
        m_aiOneShot = oneShot;
        sendAiQuery();
    }
}

// 人机对弈中随时交换双方执色；没在对局时，从当前局面开始和 AI 下（你走轮到的一方）
void MainWindow::swapSides()
{
    if (m_tsMode) return;
    if (!m_aiQueryId.isEmpty()) { m_engine->terminate(m_aiQueryId); m_aiQueryId.clear(); }
    m_aiNode = nullptr;
    m_aiOneShot = false;
    m_aiPaused = false;
    if (m_aiColor == Stone::Empty) {
        m_aiColor = opponent(m_tree.toPlay());
        m_tree.result.clear();
        setWindowTitle(T("NanoGo 围棋 - 人机对弈"));
    } else {
        m_aiColor = opponent(m_aiColor);
    }
    const Stone me = opponent(m_aiColor);
    m_tree.playerBlack = me == Stone::Black ? T("我") : QStringLiteral("NanoGo AI");
    m_tree.playerWhite = me == Stone::White ? T("我") : QStringLiteral("NanoGo AI");
    statusBar()->showMessage(me == Stone::Black ? T("现在你执黑，AI 执白") : T("现在你执白，AI 执黑"), 5000);
    autosave();
    positionChanged();
}

void MainWindow::newGame()
{
    if (m_tsMode) exitTsumego();
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_aiNode = nullptr;
    m_engine->terminateAll();
    // 换棋谱前清掉还指向旧棋谱节点的查询
    m_reviewQueryId.clear();
    m_fillQueryId.clear();
    m_reviewLine.clear();
    m_side.clear();
    m_ownQueryId.clear();
    if (m_reviewBtn) m_reviewBtn->setText(T("分析整盘棋"));
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
    const QString path = QFileDialog::getOpenFileName(this, T("打开棋谱"), QFileInfo(m_sgfPath).absolutePath(),
                                                      T("SGF 棋谱 (*.sgf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, T("打开失败"), T("无法读取文件：%1").arg(path));
        return;
    }
    loadSgfContent(Sgf::decode(f.readAll()), QFileInfo(path).fileName(), path);
}

// 换上一盘棋谱（来自文件或服务器）
bool MainWindow::loadSgfContent(const QString &text, const QString &displayName, const QString &path)
{
    if (m_tsMode) exitTsumego();
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_engine->terminateAll();
    // 换棋谱前清掉还指向旧棋谱节点的查询
    m_reviewQueryId.clear();
    m_fillQueryId.clear();
    m_reviewLine.clear();
    m_side.clear();
    m_ownQueryId.clear();
    if (m_reviewBtn) m_reviewBtn->setText(T("分析整盘棋"));
    m_queryId.clear();
    m_queryNode = nullptr;
    QString err;
    if (!Sgf::load(text, m_tree, &err)) {
        QMessageBox::warning(this, T("打开失败"), err);
        newGame();
        return false;
    }
    m_sgfPath = path;
    setWindowTitle(T("NanoGo 围棋 - %1").arg(displayName));
    positionChanged();
    return true;
}

void MainWindow::saveSgf()
{
    QString path = QFileDialog::getSaveFileName(this, T("保存棋谱"), m_sgfPath, T("SGF 棋谱 (*.sgf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".sgf", Qt::CaseInsensitive)) path += ".sgf";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, T("保存失败"), T("无法写入：%1").arg(path));
        return;
    }
    f.write(Sgf::save(m_tree).toUtf8());
    m_sgfPath = path;
    statusBar()->showMessage(T("已保存到 %1").arg(path), 5000);
}

void MainWindow::openSettings()
{
    QStringList gpuList;
    for (const GpuInfo &g : HardwareInfo::openclGpus()) gpuList << HardwareInfo::describe(g);
    if (gpuList.isEmpty()) gpuList = m_engine->openclDevices();
    SettingsDialog dlg(m_settings, gpuList, this);
    if (dlg.exec() != QDialog::Accepted) return;
    AppSettings old = m_settings;
    m_settings = dlg.result();
    if (old.modelPath != m_settings.modelPath && m_settings.autoUpdateModel) {
        // 手动换了模型：关掉自动切换到最强，免得第二天又被换回去
        m_settings.autoUpdateModel = false;
        if (m_autoUpdAct) m_autoUpdAct->setChecked(false);
        statusBar()->showMessage(T("已关闭“自动切换到官方最强模型”，以免换掉你选的模型"), 8000);
    }
    m_settings.save();
    const bool engineChanged = old.katagoPath != m_settings.katagoPath || old.modelPath != m_settings.modelPath
                               || old.gpuDevice != m_settings.gpuDevice || old.searchThreads != m_settings.searchThreads
                               || old.tunedFor != m_settings.tunedFor;
    if (old.effectiveLanguage() != m_settings.effectiveLanguage()) {
        if (QMessageBox::question(this, T("界面语言"), T("切换语言需要重新打开 NanoGo，现在重新打开吗？（当前棋局会自动保存）"))
            == QMessageBox::Yes) {
            autosave();
            m_engine->stop();
            QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
            QApplication::quit();
            return;
        }
    }
    if (engineChanged || m_engine->state() == KataGoEngine::State::Failed) restartEngine();
    else if (m_analyzeAct->isChecked()) requestAnalysis();
}

void MainWindow::restartEngine()
{
    m_queryId.clear();
    m_queryNode = nullptr;
    appendLog(T("—— 重启引擎 ——"));
    startEngine();
}

void MainWindow::onPointClicked(int x, int y)
{
    if (m_tsMode) { tsOnClick(x, y); return; }
    if (m_aiColor != Stone::Empty && m_tree.toPlay() == m_aiColor) {
        statusBar()->showMessage(m_aiPaused ? T("现在轮到 AI：按 Ctrl+Enter 让 AI 走，或按 → 前进")
                                            : T("AI 正在思考，请稍候…"), 3000);
        return;
    }
    if (m_tree.play(x, y)) { m_aiPaused = false; m_aiOneShot = false; positionChanged(); }
    else QApplication::beep();
}

void MainWindow::onEngineState(KataGoEngine::State s, const QString &msg)
{
    if (m_prepProgress && (s == KataGoEngine::State::Ready || s == KataGoEngine::State::Failed || s == KataGoEngine::State::Stopped)) {
        m_prepProgress->deleteLater();
        m_prepProgress = nullptr;
    }
    QString color = s == KataGoEngine::State::Ready ? "green" : s == KataGoEngine::State::Failed ? "red" : "#b07000";
    m_engineStatus->setText(QStringLiteral("<span style='color:%1'>●</span> %2").arg(color, msg.section('\n', 0, 0).toHtmlEscaped()));
    if (s == KataGoEngine::State::Ready && m_analyzeAct->isChecked()) requestAnalysis();
    if (s == KataGoEngine::State::Ready && !m_chosenGpuName.isEmpty() && !m_engine->deviceName().isEmpty()
        && !m_engine->deviceName().contains(m_chosenGpuName)) {
        // KataGo 实际用的卡和我们选的不一样（两边编号不一致）：按名字找到正确编号后重启
        for (const QString &d : m_engine->openclDevices()) {
            if (d.contains(m_chosenGpuName)) {
                const int idx = d.section(':', 0, 0).toInt();
                if (idx != m_gpuOverride) {
                    m_gpuOverride = idx;
                    appendLog(T("—— KataGo 用错了显卡，改用 %1 号重新启动 ——").arg(idx));
                    QTimer::singleShot(0, this, &MainWindow::restartEngine);
                    return;
                }
            }
        }
    }
    if (s == KataGoEngine::State::Ready) {
        m_aiQueryId.clear();
        m_triedBuilds.clear();
        if (m_pendingStrength != 0 && m_engine->hasHumanModel()) {
            const int v = m_pendingStrength;
            m_pendingStrength = 0;
            setAiVisits(v);
        } else if (isRank(m_aiVisits) && !m_engine->hasHumanModel()) {
            // 人类风格模型不见了：退回普通强度，免得 AI 以为在按段位下其实是全力下
            setAiVisits(400);
        }
        if (m_tsMode && m_tsState == TsState::Thinking) tsSendBest();
        maybeAiMove();
        if (m_aiColor != Stone::Empty) backfillGraph();  // 重启后把之前下的手补到胜率曲线上
    }
    // 开发用：NANOGO_AUTOREVIEW=1 时引擎一就绪就整盘分析（自动测试用）
    if (s == KataGoEngine::State::Ready && qEnvironmentVariableIsSet("NANOGO_AUTOREVIEW") && m_reviewQueryId.isEmpty() && m_reviewDone == 0)
        startReview();
    if (s == KataGoEngine::State::Failed) {
        // 硬件自动适配：当前引擎起不来（例如没有可用显卡），自动换下一个可用的引擎（显卡版 → CPU 版）。
        // 配置或模型出错时换引擎没有用，也不能因此把能用的显卡版降级，直接报告原因。
        m_triedBuilds << m_settings.katagoPath;
        QString next = m_engine->failureIsConfigOrModel() ? QString() : m_settings.nextKatagoBuild();
        while (!next.isEmpty() && m_triedBuilds.contains(next)) {
            AppSettings tmp = m_settings; tmp.katagoPath = next; next = tmp.nextKatagoBuild();
        }
        const bool configOrModel = m_engine->failureIsConfigOrModel();
        if (next.isEmpty() && !configOrModel && !AppSettings::isCpuBuild(m_settings.katagoPath)) {
            // 本机没有现成的 CPU 版：自动下载一个
            const EngineBuild cpu = EngineInstaller::cpuBuildForThisPc();
            if (!m_triedBuilds.contains("download:" + cpu.kind)) {
                m_triedBuilds << "download:" + cpu.kind;
                appendLog(msg);
                statusBar()->showMessage(T("显卡版引擎用不了，正在自动下载 CPU 版（会慢一些）"), 10000);
                installEngine(cpu, T("—— 这个引擎在本机用不了，自动改用 CPU 版 ——"));
                return;
            }
        }
        if (!next.isEmpty()) {
            appendLog(msg);
            appendLog(T("—— 这个引擎在本机用不了，自动改用：%1 ——").arg(next));
            statusBar()->showMessage(T("显卡版引擎用不了，已自动改用其他版本（可能是 CPU 版，会慢一些）"), 10000);
            m_settings.katagoPath = next;
            m_settings.save();
            QTimer::singleShot(0, this, &MainWindow::restartEngine);
            return;
        }
        QMessageBox box(QMessageBox::Warning, T("引擎出错"), msg, QMessageBox::Ok, this);
        QPushButton *settingsBtn = box.addButton(T("打开设置"), QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == static_cast<QAbstractButton *>(settingsBtn)) openSettings();
    }
}

QJsonObject MainWindow::buildQuery() const { return buildQueryFor(m_tree.pathToCurrent()); }

QJsonObject MainWindow::buildQueryFor(const QList<GameNode *> &path) const
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
    if (m_tsMode && tsHandleResponse(resp)) return;
    if (!m_aiQueryId.isEmpty() && resp.value("id").toString() == m_aiQueryId) {
        if (resp.value("isDuringSearch").toBool()) return;
        m_aiQueryId.clear();
        const bool oneShot = m_aiOneShot;
        m_aiOneShot = false;
        if (m_aiNode != m_tree.current()) return;
        // 顺手把 AI 算出的胜率记到这一手上，胜率曲线就能边下边画（不覆盖更细的分析，也不存候选点免得泄露提示）
        {
            const QJsonObject root = resp.value("rootInfo").toObject();
            Analysis &a = m_aiNode->analysis;
            if (!root.isEmpty() && !a.valid) {
                a.valid = true;
                a.winrateBlack = root.value("winrate").toDouble();
                a.scoreLeadBlack = root.value("scoreLead").toDouble();
                a.visits = root.value("visits").toInt();
                refreshReview();
            }
            backfillGraph();
        }
        if (!oneShot && (m_aiColor == Stone::Empty || m_tree.toPlay() != m_aiColor)) return;
        const QJsonArray infos = resp.value("moveInfos").toArray();
        QString best;
        const QJsonArray human = resp.value("humanPolicy").toArray();
        if (isRank(m_aiVisits) && !human.isEmpty()) {
            // 按人类段位：照人类风格模型给出的“这个水平的人会下哪里”的概率抽一手，所以会犯符合该段位的错误
            const int n = m_tree.boardSize();
            double total = 0;
            for (int i = 0; i < n * n && i < human.size(); ++i) total += qMax(0.0, human[i].toDouble());
            double r = QRandomGenerator::global()->generateDouble() * total;
            int pick = -1;
            for (int i = 0; i < n * n && i < human.size(); ++i) {
                const double p = qMax(0.0, human[i].toDouble());
                if (p <= 0) continue;
                pick = i;
                if ((r -= p) <= 0) break;
            }
            // 人类模型最想停一手（收完官子）时才停
            const double passP = human.size() > n * n ? human[n * n].toDouble() : 0;
            if (pick < 0 || passP > 0.5) best = QStringLiteral("pass");
            else best = BoardWidget::toGtp(pick % n, pick / n, n);
        } else if (!infos.isEmpty()) {
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
        if (!oneShot && aiWr < 0.03 && m_tree.current()->depth() > 100) {
            m_tree.result = m_aiColor == Stone::Black ? QStringLiteral("W+R") : QStringLiteral("B+R");
            m_aiColor = Stone::Empty;
            refreshInfo();
            QMessageBox::information(this, T("对局结束"), T("AI 认输，你赢了！"));
            return;
        }
        // 对方刚停了一手：如果停一手不吃亏，AI 也停，好让对局结束
        const Move &prev = m_tree.current()->move;
        if (prev.isValid() && prev.isPass()) {
            // 全盘每个点的归属都已确定（中国规则下已无单官可收），停一手
            const QJsonArray own = resp.value("ownership").toArray();
            bool settled = own.size() == m_tree.boardSize() * m_tree.boardSize();
            for (const auto &v : own) if (qAbs(v.toDouble()) < 0.5) { settled = false; break; }
            if (settled) best = QStringLiteral("pass");
            if (!infos.isEmpty()) {
                const double bestLead = infos.first().toObject().value("scoreLead").toDouble();
                for (const auto &v : infos) {
                    const QJsonObject mi = v.toObject();
                    if (mi.value("move").toString().compare("pass", Qt::CaseInsensitive) != 0) continue;
                    const double diff = m_tree.toPlay() == Stone::Black ? bestLead - mi.value("scoreLead").toDouble()
                                                                        : mi.value("scoreLead").toDouble() - bestLead;
                    if (diff < 0.5) best = QStringLiteral("pass");
                }
            } else {
                const QJsonArray policy = resp.value("policy").toArray();
                const int n = m_tree.boardSize();
                if (policy.size() > n * n && policy[n * n].toDouble() >= 0.05) best = QStringLiteral("pass");
            }
        }
        int x, y;
        if (!(BoardWidget::fromGtp(best, m_tree.boardSize(), &x, &y) && m_tree.play(x, y))) m_tree.pass();
        positionChanged();
        return;
    }
    const QString rid = resp.value("id").toString();
    if (!m_fillQueryId.isEmpty() && rid == m_fillQueryId) {
        if (resp.value("isDuringSearch").toBool()) return;
        const int turn = resp.value("turnNumber").toInt(-1);
        if (turn >= 0 && turn < m_fillLine.size()) {
            const QJsonObject root = resp.value("rootInfo").toObject();
            Analysis &a = m_fillLine[turn]->analysis;
            if (!a.valid && !root.isEmpty()) {
                a.valid = true;
                a.winrateBlack = root.value("winrate").toDouble();
                a.scoreLeadBlack = root.value("scoreLead").toDouble();
                a.visits = root.value("visits").toInt();
            }
        }
        if (--m_fillLeft <= 0) { m_fillQueryId.clear(); m_fillLine.clear(); }
        refreshReview();
        return;
    }
    if (!m_reviewQueryId.isEmpty() && rid == m_reviewQueryId) {
        if (resp.value("isDuringSearch").toBool()) return;
        const int turn = resp.value("turnNumber").toInt(-1);
        if (turn >= 0 && turn < m_reviewLine.size()) {
            const QJsonObject root = resp.value("rootInfo").toObject();
            Analysis &a = m_reviewLine[turn]->analysis;
            if (!a.valid || a.visits <= root.value("visits").toInt()) {
                a.valid = true;
                a.winrateBlack = root.value("winrate").toDouble();
                a.scoreLeadBlack = root.value("scoreLead").toDouble();
                a.visits = root.value("visits").toInt();
                a.raw = resp;
            }
        }
        if (++m_reviewDone >= m_reviewLine.size()) {
            m_reviewQueryId.clear();
    m_fillQueryId.clear();
            m_reviewBtn->setText(T("分析整盘棋"));
            statusBar()->showMessage(T("整盘分析完成"), 5000);
            autosave();
        }
        refreshInfo();
        m_board->update();
        return;
    }
    if (m_side.contains(rid)) {
        if (resp.value("isDuringSearch").toBool()) return;
        const SideQuery sq = m_side.take(rid);
        if (rid == m_ownQueryId) m_ownQueryId.clear();
        const QJsonObject root = resp.value("rootInfo").toObject();
        Analysis &a = sq.node->analysis;
        if (!a.valid || a.visits <= root.value("visits").toInt() || !a.raw.contains("ownership")) {
            a.valid = true;
            a.winrateBlack = root.value("winrate").toDouble();
            a.scoreLeadBlack = root.value("scoreLead").toDouble();
            a.visits = root.value("visits").toInt();
            a.raw = resp;
        }
        if (sq.node == m_tree.current()) { m_board->update(); refreshInfo(); }
        if (sq.purpose == 1) showCount(sq.node, resp);
        return;
    }
    if (rid != m_queryId || !m_queryNode) return;
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
    if (m_tsMode) { tsAfterNavigate(); return; }
    autosave();
    m_pinnedMove.clear();
    m_board->setPinnedPv({});
    m_comment->blockSignals(true);
    m_comment->setPlainText(m_tree.current()->comment);
    m_comment->blockSignals(false);
    m_board->update();
    refreshInfo();
    requestAnalysis();
    // 没开连续分析时，每个局面也快速算一次（约 200 次），用于参考图、上一手评价和形势
    if (!m_analyzeAct->isChecked() && !m_tree.current()->analysis.raw.contains("moveInfos")) {
        if (!m_ownQueryId.isEmpty()) { m_engine->terminate(m_ownQueryId); m_side.remove(m_ownQueryId); }
        sendSideQuery(0, 200);
    }

    if (m_aiColor != Stone::Empty) {
        const GameNode *cur = m_tree.current();
        if (cur->move.isPass() && cur->move.isValid() && cur->parent && cur->parent->move.isValid() && cur->parent->move.isPass()) {
            statusBar()->showMessage(T("双方都停了一手，正在点目…"), 5000);
            countScore();
            return;
        }
        maybeAiMove();
    }
}

void MainWindow::maybeAiMove()
{
    if (m_aiColor == Stone::Empty || m_tree.toPlay() != m_aiColor) return;
    if (m_aiPaused) {
        statusBar()->showMessage(T("AI 已暂停：你落子后继续，或按 Ctrl+Enter 让 AI 走这一步"), 5000);
        return;
    }
    if (m_engine->state() != KataGoEngine::State::Ready) {
        statusBar()->showMessage(T("等引擎就绪后 AI 会自动落子"), 3000);
        return;
    }
    if (!m_aiQueryId.isEmpty() && m_aiNode == m_tree.current()) return; // 已经在算了
    sendAiQuery();
}

void MainWindow::sendAiQuery()
{
    if (m_tsMode) return;
    if (!m_aiQueryId.isEmpty()) m_engine->terminate(m_aiQueryId);
    QJsonObject q = buildQuery();
    q["maxVisits"] = isRank(m_aiVisits) ? 1 : m_aiVisits;
    q.remove("reportDuringSearchEvery");
    if (isRank(m_aiVisits) && m_engine->hasHumanModel()) {
        QJsonObject o = q.value("overrideSettings").toObject();
        o["humanSLProfile"] = rankProfile(m_aiVisits);
        q["overrideSettings"] = o;
    }
    // 对方刚停一手时要看全盘归属，判断该不该跟着停
    q["includeOwnership"] = m_tree.current()->move.isValid() && m_tree.current()->move.isPass();
    q["includePolicy"] = true;
    m_aiNode = m_tree.current();
    m_aiQueryId = m_engine->analyze(q);
    statusBar()->showMessage(T("AI 思考中…"), 2000);
}

void MainWindow::newAiGame()
{
    if (m_tsMode) exitTsumego();
    QDialog dlg(this);
    dlg.setWindowTitle(T("人机对弈"));
    auto *form = new QFormLayout(&dlg);
    auto *color = new QComboBox(&dlg);
    color->addItem(T("我执黑（先走）"), int(Stone::Black));
    color->addItem(T("我执白"), int(Stone::White));
    form->addRow(T("我的颜色："), color);
    auto *handicap = new QSpinBox(&dlg);
    handicap->setRange(0, 9);
    handicap->setSuffix(T(" 子"));
    handicap->setToolTip(T("让子时黑方先摆子，白方先走，贴目自动改为 0.5"));
    form->addRow(T("让子（黑方摆子）："), handicap);
    auto *strength = new QComboBox(&dlg);
    fillStrengthBox(strength, true);
    strength->setCurrentIndex(qMax(0, strength->findData(m_aiVisits)));
    form->addRow(T("AI 强度："), strength);
    auto *note = new QLabel(T("提示：按计算量的选项即使只算 1 次也有职业以上水平。\n想下得过，选“像野狐 X 级/段那样下”：AI 模仿该水平的人类，第一次选要下载约 95 MB 的模型。"), &dlg);
    note->setStyleSheet("color: gray;");
    form->addRow(note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(T("开始"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    newGame();
    const Stone me = Stone(color->currentData().toInt());
    setAiVisits(strength->currentData().toInt());
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
    m_tree.playerBlack = me == Stone::Black ? T("我") : QStringLiteral("NanoGo AI");
    m_tree.playerWhite = me == Stone::White ? T("我") : QStringLiteral("NanoGo AI");
    m_tree.setCurrent(m_tree.root());
    m_aiColor = opponent(me);
    m_aiPaused = false;
    setWindowTitle(T("NanoGo 围棋 - 人机对弈"));
    positionChanged();
}

// 胜率曲线上缺的手（重启前下的、算到一半被取消的）用很少的计算量在后台补上
void MainWindow::backfillGraph()
{
    if (m_tsMode || !m_fillQueryId.isEmpty() || !m_reviewQueryId.isEmpty()) return;
    if (m_engine->state() != KataGoEngine::State::Ready) return;
    const QList<GameNode *> line = m_tree.mainLineFromCurrentPath();
    QList<GameNode *> moveNodes{line.first()};
    for (int i = 1; i < line.size(); ++i) {
        if (!line[i]->addBlack.isEmpty() || !line[i]->addWhite.isEmpty()) return;
        if (line[i]->move.isValid()) moveNodes << line[i];
    }
    QJsonArray turns;
    for (int t = 0; t < moveNodes.size(); ++t)
        if (!moveNodes[t]->analysis.valid && moveNodes[t] != m_tree.current()) turns.append(t);
    if (turns.isEmpty()) return;
    QJsonObject q = buildQueryFor(moveNodes);
    q.remove("reportDuringSearchEvery");
    q["maxVisits"] = 100;
    q["includeOwnership"] = false;
    q["analyzeTurns"] = turns;
    m_fillQueryId = m_engine->analyze(q);
    if (m_fillQueryId.isEmpty()) return;
    m_fillLine = moveNodes;
    m_fillLeft = turns.size();
}

void MainWindow::startReview()
{
    if (!m_reviewQueryId.isEmpty()) {
        // 再按一次：停止
        m_engine->terminate(m_reviewQueryId);
        m_reviewQueryId.clear();
    m_fillQueryId.clear();
        m_reviewBtn->setText(T("分析整盘棋"));
        statusBar()->showMessage(T("已停止整盘分析"), 4000);
        refreshInfo();
        return;
    }
    if (m_engine->state() != KataGoEngine::State::Ready) {
        statusBar()->showMessage(T("引擎还没就绪，请稍候再试"), 3000);
        return;
    }
    const QList<GameNode *> line = m_tree.mainLineFromCurrentPath();
    QList<GameNode *> moveNodes{line.first()};
    for (int i = 1; i < line.size(); ++i) {
        if (!line[i]->addBlack.isEmpty() || !line[i]->addWhite.isEmpty()) {
            QMessageBox::information(this, T("分析整盘棋"), T("这盘棋中途有摆子，暂时不能整盘分析，可以逐手打开分析。"));
            return;
        }
        if (line[i]->move.isValid()) moveNodes << line[i];
    }
    if (moveNodes.size() < 2) {
        statusBar()->showMessage(T("还没有着手可以分析"), 3000);
        return;
    }
    QJsonObject q = buildQueryFor(moveNodes);
    q.remove("reportDuringSearchEvery");
    q["maxVisits"] = m_reviewVisits->currentData().toInt();
    QJsonArray turns;
    for (int t = 0; t < moveNodes.size(); ++t) turns.append(t);
    q["analyzeTurns"] = turns;
    m_reviewLine = moveNodes;
    m_reviewDone = 0;
    m_reviewQueryId = m_engine->analyze(q);
    if (m_reviewQueryId.isEmpty()) return;
    m_reviewBtn->setText(T("停止分析"));
    refreshInfo();
}

void MainWindow::refreshReview()
{
    if (!m_graph) return;
    const QList<GameNode *> line = m_tree.mainLineFromCurrentPath();
    m_evals = Review::evaluate(line);
    m_graph->setData(line, m_evals, m_tree.current(), m_tree.boardSize());

    int bad[2][2] = {{0, 0}, {0, 0}};
    int known = 0;
    for (const MoveEval &e : m_evals) {
        if (!e.known) continue;
        ++known;
        if (e.severity > 0) ++bad[e.color == Stone::Black ? 0 : 1][e.severity - 1];
    }
    QString info;
    if (!m_reviewQueryId.isEmpty())
        info = T("正在分析… %1 / %2").arg(m_reviewDone).arg(m_reviewLine.size()) + " · ";
    if (known > 0)
        info += T("黑：<b style='color:#d33'>恶手 %1</b> · <b style='color:#d80'>疑问手 %2</b>　白：<b style='color:#d33'>恶手 %3</b> · <b style='color:#d80'>疑问手 %4</b>")
                    .arg(bad[0][1]).arg(bad[0][0]).arg(bad[1][1]).arg(bad[1][0]);
    m_reviewInfo->setText(info);

    // 棋盘上标出当前局面里还在的坏棋
    QHash<int, int> marks;
    const int n = m_tree.boardSize();
    const QList<GameNode *> path = m_tree.pathToCurrent();
    for (const MoveEval &e : m_evals) {
        if (!e.known || e.severity == 0 || e.node->move.isPass() || !path.contains(e.node)) continue;
        const Move &mv = e.node->move;
        if (m_tree.board().at(mv.x, mv.y) == mv.color) marks.insert(mv.y * n + mv.x, e.severity);
    }
    m_board->setMoveMarks(marks);
}

void MainWindow::jumpToMistake(int dir)
{
    const QList<GameNode *> line = m_tree.mainLineFromCurrentPath();
    const int cur = line.indexOf(m_tree.current());
    QList<int> idx;
    for (const MoveEval &e : m_evals) if (e.known && e.severity > 0) idx << e.moveNo;
    if (idx.isEmpty()) {
        statusBar()->showMessage(T("还没有找到坏棋：先点“分析整盘棋”"), 4000);
        return;
    }
    int target = -1;
    if (dir > 0) { for (int i : idx) if (i > cur) { target = i; break; } }
    else { for (int k = idx.size() - 1; k >= 0; --k) if (idx[k] < cur) { target = idx[k]; break; } }
    if (target < 0 || target >= line.size()) {
        statusBar()->showMessage(dir > 0 ? T("后面没有坏棋了") : T("前面没有坏棋了"), 3000);
        return;
    }
    m_aiPaused = m_aiColor != Stone::Empty;
    m_tree.setCurrent(line[target]);
    positionChanged();
    const MoveEval &e = m_evals[target - 1];
    statusBar()->showMessage(T("第 %1 手 %2：损失 %3 目（胜率 %4%）").arg(target)
                                 .arg(e.severity == 2 ? T("恶手") : T("疑问手"))
                                 .arg(e.scoreLoss, 0, 'f', 1).arg(e.winLoss * 100, 0, 'f', 1), 8000);
}

void MainWindow::refreshInfo()
{
    if (m_tsMode) { tsRefresh(); return; }
    refreshReference();
    refreshReview();
    const GameNode *cur = m_tree.current();
    const int moveNo = cur->depth();
    const QString toPlay = m_tree.toPlay() == Stone::Black ? T("黑") : T("白");
    const QString pb = m_tree.playerBlack.isEmpty() ? T("黑方") : m_tree.playerBlack.toHtmlEscaped();
    const QString pw = m_tree.playerWhite.isEmpty() ? T("白方") : m_tree.playerWhite.toHtmlEscaped();
    QString lastMove;
    if (cur->move.isValid())
        lastMove = T("，上一手 %1 %2").arg(cur->move.color == Stone::Black ? T("黑") : T("白"),
                                                     BoardWidget::toGtp(cur->move.x, cur->move.y, m_tree.boardSize()));
    m_gameInfo->setText(T("<b>%1</b> 对 <b>%2</b><br>第 %3 手%4<br>轮到 %5 走 · 贴 %6 目<br>提子：黑 %7 · 白 %8%9")
                            .arg(pb, pw).arg(moveNo).arg(lastMove, toPlay).arg(m_tree.komi())
                            .arg(m_tree.board().capturesBlack()).arg(m_tree.board().capturesWhite())
                            .arg(m_tree.result.isEmpty() ? QString() : T("<br>结果：%1").arg(m_tree.result.toHtmlEscaped())));

    QString turn;
    if (m_aiColor != Stone::Empty) {
        if (m_tree.toPlay() != m_aiColor) turn = T("<b style='color:#2a7'>轮到你走</b>");
        else if (m_aiPaused) turn = T("<b style='color:#b70'>AI 已暂停</b>：落子或按 Ctrl+Enter 继续");
        else turn = T("<b style='color:#27a'>AI 思考中…</b>");
        turn += "<br>";
    } else {
        // 没有在和 AI 下：双方都由你摆，提示怎样开始对弈
        turn = T("<span style='color:#888;font-size:small'>摆棋/复盘模式：黑白都由你下。想和 AI 下，点“人机对弈”或“交换黑白”。</span><br>");
    }
    const Analysis &a = cur->analysis;
    if (!a.valid) {
        m_analysisInfo->setText(turn + (m_analyzeAct->isChecked() ? T("计算中…") : T("按空格键开始分析")));
        return;
    }
    const double lead = a.scoreLeadBlack;
    const QString leader = lead >= 0 ? T("黑") : T("白");
    m_analysisInfo->setText(turn + T("黑胜率 <b>%1%</b><br>%2 领先 <b>%3</b> 目<br><span style='color:gray'>计算量 %4</span>")
                                .arg(a.winrateBlack * 100, 0, 'f', 1).arg(leader).arg(qAbs(lead), 0, 'f', 1).arg(a.visits));
}

void MainWindow::chooseModel()
{
    if (m_updater->busy()) {
        QMessageBox::information(this, T("选择模型"), T("正在检查或下载中，请稍候。"));
        return;
    }
    if (!m_officialModels.isEmpty()) { openModelChooser(m_officialModels); return; }
    m_choosingModel = true;
    statusBar()->showMessage(T("正在从 katagotraining.org 读取模型列表…"), 5000);
    m_updater->checkForUpdates();
}

void MainWindow::openModelChooser(const QList<NetworkInfo> &list)
{
    ModelChooser dlg(list, m_settings.modelPath, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const NetworkInfo best = ModelUpdater::strongest(list);
    const QString bestFile = best.name.isEmpty() ? QString() : QFileInfo(QUrl(best.url).path()).fileName();
    auto disableAutoUpdate = [&](const QString &file) {
        if (file == bestFile || !m_settings.autoUpdateModel) return;
        m_settings.autoUpdateModel = false;
        if (m_autoUpdAct) m_autoUpdAct->setChecked(false);
        statusBar()->showMessage(T("已关闭“自动切换到官方最强模型”，以免换掉你选的模型"), 8000);
    };
    if (!dlg.localPath().isEmpty()) {
        const QString path = dlg.localPath();
        disableAutoUpdate(QFileInfo(path).fileName());
        if (QFileInfo(path).absoluteFilePath() == QFileInfo(m_settings.modelPath).absoluteFilePath()) { m_settings.save(); return; }
        m_settings.modelPath = path;
        m_settings.save();
        appendLog(T("—— 切换模型：%1 ——").arg(path));
        restartEngine();
        return;
    }
    const NetworkInfo n = dlg.network();
    if (n.name.isEmpty()) return;
    disableAutoUpdate(QFileInfo(QUrl(n.url).path()).fileName());
    m_settings.save();
    m_manualModelDownload = true;
    appendLog(T("下载所选模型：") + n.name);
    m_updater->download(n);
}

void MainWindow::checkModel(bool manual)
{
    if (m_updater->busy()) {
        if (manual) QMessageBox::information(this, T("模型更新"), T("正在检查或下载中，请稍候。"));
        return;
    }
    m_manualCheck = manual;
    statusBar()->showMessage(T("正在从 katagotraining.org 检查最新模型…"), 5000);
    m_updater->checkForUpdates();
}

void MainWindow::onModelList(const QList<NetworkInfo> &list)
{
    m_settings.lastModelCheck = QDateTime::currentDateTime();
    m_settings.save();
    m_officialModels = list;
    if (m_choosingModel) {
        m_choosingModel = false;
        openModelChooser(list);
        return;
    }
    const NetworkInfo best = ModelUpdater::strongest(list);
    if (best.name.isEmpty()) {
        if (m_manualCheck) QMessageBox::warning(this, T("模型更新"), T("没有从官方列表里找到可用的模型。"));
        return;
    }
    if (m_firstModel) {
        appendLog(T("第一次使用，下载官方最强模型：") + best.name);
        m_updater->download(best);
        return;
    }
    const NetworkInfo *cur = ModelUpdater::find(list, m_settings.modelPath);
    auto desc = [](const NetworkInfo &n) {
        return T("%1\n等级分 %2 ± %3（%4 局评测）").arg(n.name).arg(n.elo, 0, 'f', 0).arg(n.eloUncertainty, 0, 'f', 0).arg(n.games);
    };
    appendLog(T("官方最强模型：") + desc(best).replace('\n', ' '));

    if (cur && (cur->name == best.name || cur->eloLower >= best.eloLower)) {
        if (m_manualCheck)
            QMessageBox::information(this, T("模型更新"), T("当前用的已经是最强模型：\n%1").arg(desc(*cur)));
        else statusBar()->showMessage(T("当前模型已是官方最强"), 5000);
        return;
    }

    const QString sizeMb = QString::number(best.bytes >> 20);
    // 当前模型在官方列表里且开了自动更新：直接下载切换；否则先问
    if (!(cur && m_settings.autoUpdateModel)) {
        const QString curText = cur ? desc(*cur) : T("%1（不在官方列表里，无法比较）").arg(QFileInfo(m_settings.modelPath).fileName());
        auto ans = QMessageBox::question(this, T("发现更强的模型"),
            T("官方最强模型：\n%1\n\n当前模型：\n%2\n\n要下载（约 %3 MB）并切换吗？旧模型会保留。")
                .arg(desc(best), curText, sizeMb));
        if (ans != QMessageBox::Yes) return;
    } else {
        appendLog(T("自动更新：%1 → %2（高 %3 分）").arg(cur->name, best.name).arg(best.elo - cur->elo, 0, 'f', 0));
    }
    statusBar()->showMessage(T("开始下载新模型 %1（%2 MB）…").arg(best.name, sizeMb), 5000);
    m_updater->download(best);
}

// 自动保存：每走一步都把整盘棋、当前位置和人机对弈状态写到 data 目录，下次打开自动恢复
void MainWindow::autosave()
{
    if (m_tsMode) return; // 做题时不覆盖保存的棋局
    const QString dir = AppSettings::dataDir();
    QFile f(QDir(dir).filePath("autosave.sgf"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(Sgf::save(m_tree).toUtf8());
    QStringList path;
    for (GameNode *n : m_tree.pathToCurrent())
        if (n->parent) path << QString::number(n->parent->children.indexOf(n));
    QSettings st(QDir(dir).filePath("autosave.ini"), QSettings::IniFormat);
    st.setValue("path", path.join(','));
    st.setValue("aiColor", int(m_aiColor));
    st.setValue("aiVisits", m_aiVisits);
    st.setValue("sgfPath", m_sgfPath);
}

void MainWindow::restoreAutosave()
{
    const QString dir = AppSettings::dataDir();
    QFile f(QDir(dir).filePath("autosave.sgf"));
    if (!f.open(QIODevice::ReadOnly)) return;
    QString err;
    if (!Sgf::load(Sgf::decode(f.readAll()), m_tree, &err)) return;
    QSettings st(QDir(dir).filePath("autosave.ini"), QSettings::IniFormat);
    GameNode *n = m_tree.root();
    for (const QString &i : st.value("path").toString().split(',', Qt::SkipEmptyParts)) {
        const int idx = i.toInt();
        if (idx < 0 || idx >= n->children.size()) break;
        n = n->children[idx];
    }
    m_tree.setCurrent(n);
    m_aiColor = Stone(st.value("aiColor", 0).toInt());
    m_aiVisits = st.value("aiVisits", m_aiVisits).toInt();
    if (m_strengthBox) {
        const int i = m_strengthBox->findData(m_aiVisits);
        if (i >= 0) m_strengthBox->setCurrentIndex(i);
    }
    m_sgfPath = st.value("sgfPath").toString();
    if (m_aiColor != Stone::Empty) setWindowTitle(T("NanoGo 围棋 - 人机对弈"));
    else if (!m_sgfPath.isEmpty()) setWindowTitle(T("NanoGo 围棋 - %1").arg(QFileInfo(m_sgfPath).fileName()));
}

void MainWindow::sendSideQuery(int purpose, int visits)
{
    if (m_engine->state() != KataGoEngine::State::Ready) {
        statusBar()->showMessage(T("引擎还没就绪，请稍候再试"), 3000);
        return;
    }
    QJsonObject q = buildQuery();
    q["maxVisits"] = visits;
    q.remove("reportDuringSearchEvery");
    q["includeOwnership"] = true;
    const QString id = m_engine->analyze(q);
    if (id.isEmpty()) return;
    m_side.insert(id, SideQuery{m_tree.current(), purpose});
    if (purpose == 0) m_ownQueryId = id;
}

void MainWindow::countScore()
{
    if (m_engine->state() != KataGoEngine::State::Ready) {
        QMessageBox::information(this, T("点目"), T("引擎还没就绪，请稍候再点目。"));
        return;
    }
    if (!m_aiQueryId.isEmpty()) { m_engine->terminate(m_aiQueryId); m_aiQueryId.clear(); }
    m_aiPaused = true;
    m_ownAct->setChecked(true); // 点目时显示地盘归属
    statusBar()->showMessage(T("正在点目…"), 5000);
    sendSideQuery(1, 800);
}

void MainWindow::showCount(GameNode *node, const QJsonObject &resp)
{
    const int n = m_tree.boardSize();
    const QJsonArray own = resp.value("ownership").toArray();
    if (own.size() != n * n) {
        QMessageBox::warning(this, T("点目"), T("引擎没有返回地盘信息，无法点目。"));
        return;
    }
    // 数子法（中国规则）：活子 + 围住的空点。归属不明（|v| < 0.5）的点算单官，不计。
    int black = 0, white = 0, unsure = 0, deadB = 0, deadW = 0;
    const Board &b = m_tree.board();
    for (int i = 0; i < n * n; ++i) {
        const double v = own[i].toDouble();
        const Stone st = b.at(i % n, i / n);
        if (v > 0.5) { ++black; if (st == Stone::White) ++deadW; }
        else if (v < -0.5) { ++white; if (st == Stone::Black) ++deadB; }
        else ++unsure;
    }
    const double komi = m_tree.komi();
    const double diff = black - (white + komi);
    const QString areaResult = diff > 0 ? T("黑胜 %1 目").arg(diff) : diff < 0 ? T("白胜 %1 目").arg(-diff) : T("和棋");
    const double lead = resp.value("rootInfo").toObject().value("scoreLead").toDouble();
    const QString rulesName = m_tree.rules() == "japanese" ? T("日本规则") : m_tree.rules() == "korean" ? T("韩国规则")
                            : m_tree.rules() == "chinese" ? T("中国规则") : m_tree.rules();
    QString msg = T("数子结果（中国规则）：\n黑 %1 子，白 %2 子，白贴 %3 目\n%4\n").arg(black).arg(white).arg(komi).arg(areaResult);
    if (deadB || deadW) msg += T("（判为死子：黑 %1 个，白 %2 个，已算给对方）\n").arg(deadB).arg(deadW);
    if (unsure) msg += T("还有 %1 个点归属不明（单官或未下完的地方），没计入。\n").arg(unsure);
    msg += T("\nKataGo 按%1估计：%2 领先 %3 目").arg(rulesName, lead >= 0 ? T("黑") : T("白")).arg(qAbs(lead), 0, 'f', 1);

    const bool inGame = m_aiColor != Stone::Empty;
    if (inGame || m_tree.result.isEmpty()) {
        msg += T("\n\n要结束对局并记录这个结果吗？");
        if (QMessageBox::question(this, T("点目"), msg) == QMessageBox::Yes) {
            m_tree.result = diff > 0 ? QStringLiteral("B+%1").arg(diff) : diff < 0 ? QStringLiteral("W+%1").arg(-diff) : QStringLiteral("0");
            m_aiColor = Stone::Empty;
            m_aiQueryId.clear();
            setWindowTitle(T("NanoGo 围棋 - 对局已结束"));
            autosave();
            refreshInfo();
        } else if (inGame) {
            statusBar()->showMessage(T("对局继续：落子后 AI 接着下"), 5000);
        }
    } else {
        QMessageBox::information(this, T("点目"), msg);
    }
    Q_UNUSED(node);
}

void MainWindow::refreshReference()
{
    const int n = m_tree.boardSize();
    const GameNode *cur = m_tree.current();
    const bool blackToPlay = m_tree.toPlay() == Stone::Black;
    auto colorName = [](Stone s) { return s == Stone::Black ? T("黑") : T("白"); };

    // 上一手和 AI 推荐的比较
    QString last;
    if (cur->move.isValid() && cur->parent && cur->parent->analysis.raw.contains("moveInfos")) {
        const QJsonArray infos = cur->parent->analysis.raw.value("moveInfos").toArray();
        const bool moverBlack = cur->move.color == Stone::Black;
        const QString played = BoardWidget::toGtp(cur->move.x, cur->move.y, n);
        if (!infos.isEmpty()) {
            const QJsonObject best = infos.first().toObject();
            const QString bestMove = best.value("move").toString();
            const double bestLead = best.value("scoreLead").toDouble();
            last = T("上一手 %1 %2").arg(colorName(cur->move.color), played);
            if (bestMove.compare(played, Qt::CaseInsensitive) == 0) {
                last += T("：<b style='color:#2a7'>和 AI 推荐一致</b>");
            } else {
                double loss = -1;
                for (const auto &v : infos) {
                    const QJsonObject mi = v.toObject();
                    if (mi.value("move").toString().compare(played, Qt::CaseInsensitive) == 0) {
                        loss = moverBlack ? bestLead - mi.value("scoreLead").toDouble() : mi.value("scoreLead").toDouble() - bestLead;
                    }
                }
                if (loss < 0 && cur->analysis.valid) // 不在候选里：用这一手之后的局面估算
                    loss = moverBlack ? bestLead - cur->analysis.scoreLeadBlack : cur->analysis.scoreLeadBlack - bestLead;
                last += T("，AI 推荐 <b>%1</b>").arg(bestMove);
                if (loss >= 0) {
                    const QString col = loss < 1 ? "#2a7" : loss < 3 ? "#b90" : "#c33";
                    last += T("，<span style='color:%1'>损失约 %2 目</span>").arg(col).arg(qMax(0.0, loss), 0, 'f', 1);
                }
            }
        }
    }
    m_lastMoveRef->setText(last);
    m_lastMoveRef->setVisible(!last.isEmpty());

    // 当前局面的推荐下法
    m_refList->clear();
    const QJsonObject raw = cur->analysis.raw;
    if (!raw.contains("moveInfos")) {
        auto *it = new QListWidgetItem(T("点这里（或按 V）计算这一手的参考图"), m_refList);
        it->setData(Qt::UserRole, "__compute__");
        return;
    }
    const QJsonArray infos = raw.value("moveInfos").toArray();
    double bestLead = 0;
    int row = 0;
    for (const auto &v : infos) {
        const QJsonObject mi = v.toObject();
        const QString move = mi.value("move").toString();
        double wr = mi.value("winrate").toDouble();
        double lead = mi.value("scoreLead").toDouble();
        if (!blackToPlay) { wr = 1 - wr; lead = -lead; }
        if (row == 0) bestLead = lead;
        if (row > 0 && bestLead - lead > 8) continue; // 太差的不列
        QStringList pv;
        for (const auto &p : mi.value("pv").toArray()) pv << p.toString();
        QString pvText = pv.mid(0, 10).join(' ');
        if (pv.size() > 10) pvText += T(" …");
        auto *it = new QListWidgetItem(T("%1. %2  胜率 %3%  %4%5 目  （算 %6 次）\n    变化：%7")
                                           .arg(row + 1).arg(move).arg(wr * 100, 0, 'f', 1)
                                           .arg(lead >= 0 ? "+" : "").arg(lead, 0, 'f', 1)
                                           .arg(mi.value("visits").toInt()).arg(pvText), m_refList);
        it->setData(Qt::UserRole, move);
        it->setData(Qt::UserRole + 1, pv);
        if (move == m_pinnedMove) {
            it->setSelected(true);
            m_board->setPinnedPv(pv); // 分析继续时更新变化
        }
        if (++row >= 6) break;
    }
}

void MainWindow::startEngine()
{
    AppSettings run = m_settings;
    const auto lower = [](const QString &p) { return p.toLower(); };
    const bool openclBuild = AppSettings::katagoBackend(run.katagoPath) == "opencl";
    const bool cpuBuild = AppSettings::isCpuBuild(run.katagoPath);
    m_chosenGpuName.clear();

    if (openclBuild) {
        QString why;
        const QList<GpuInfo> gpus = HardwareInfo::openclGpus(&why);
        for (const GpuInfo &g : gpus) appendLog(T("检测到显卡 ") + HardwareInfo::describe(g));
        for (const DisplayAdapter &a : HardwareInfo::displayAdapters())
            appendLog(T("系统显卡：%1（专用显存 %2 MB）").arg(a.name).arg(a.dedicatedMB));
        if (gpus.isEmpty()) {
            // 没有可用显卡：直接换 CPU 版，不去让 KataGo 报错
            appendLog(why);
            QString cpu;
            for (const QString &b : AppSettings::findKatagoBuilds()) {
                const QString l = b.toLower();
                const bool avx2 = l.contains("avx2");
                if (!(l.contains("eigen") || l.contains("cpu"))) continue;
                if (avx2 && !HardwareInfo::cpuHasAvx2()) continue;
                cpu = b;
                break;
            }
            if (!cpu.isEmpty()) {
                statusBar()->showMessage(why, 15000);
                m_settings.katagoPath = cpu;
                m_settings.save();
                run.katagoPath = cpu;
            } else {
                statusBar()->showMessage(why, 15000);
                installEngine(EngineInstaller::cpuBuildForThisPc(), T("没有可用的显卡，下载 CPU 版 KataGo"));
                return;
            }
        } else if (run.gpuDevice < 0) {
            // 自动选卡：独立显卡、显存大的优先，明确告诉 KataGo 用哪块
            const GpuInfo &best = gpus[HardwareInfo::bestGpu(gpus)];
            m_chosenGpuName = best.name;
            run.gpuName = m_settings.gpuName = best.name;
            run.gpuDevice = m_gpuOverride >= 0 ? m_gpuOverride : best.clIndex;
            appendLog(T("—— 自动选用显卡 %1 ——").arg(HardwareInfo::describe(best)));
            if (best.integrated)
                appendLog(T("提示：只找到集成显卡，速度有限。"));
        } else {
            // 手动指定的显卡：记下型号，用来分别保存调优结果并核对 KataGo 实际用的卡
            for (const GpuInfo &g : gpus) {
                if (g.clIndex != run.gpuDevice) continue;
                m_chosenGpuName = g.name;
                run.gpuName = m_settings.gpuName = g.name;
                appendLog(T("—— 使用手动选择的显卡 %1 ——").arg(HardwareInfo::describe(g)));
            }
            if (m_gpuOverride >= 0) run.gpuDevice = m_gpuOverride;
        }
    } else if (cpuBuild && lower(run.katagoPath).contains("avx2") && !HardwareInfo::cpuHasAvx2()) {
        // 这台 CPU 不支持 AVX2：换通用 CPU 版，免得一启动就崩溃
        bool found = false;
        for (const QString &b : AppSettings::findKatagoBuilds()) {
            const QString l = b.toLower();
            if ((l.contains("eigen") || l.contains("cpu")) && !l.contains("avx2")) {
                m_settings.katagoPath = b; m_settings.save(); run.katagoPath = b;
                found = true;
                break;
            }
        }
        if (!found) { installEngine(EngineInstaller::build("eigen"), T("这台电脑的 CPU 不支持 AVX2，下载通用 CPU 版 KataGo")); return; }
    }
    // 测速结果按 引擎|模型|显卡 分别记住（见 AppSettings::tuneKey），换了任何一样都会重新测
    m_engine->start(run);
}
