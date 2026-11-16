#pragma once
#include "AppSettings.h"
#include "GameTree.h"
#include "KataGoEngine.h"
#include "ModelUpdater.h"
#include "Review.h"
#include "EngineInstaller.h"
#include "Tsumego.h"
#include "online/OnlineDialogs.h"
#include <QJsonArray>
#include <QRect>
#include <QHash>
#include <QMainWindow>

class BoardWidget;
class QLabel;
class QPlainTextEdit;
class QAction;
class QListWidget;
class QProgressDialog;
class QComboBox;

class MainWindow : public QMainWindow, public OnlineHost {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 在线功能（MainWindowOnline.cpp）
    QString currentGameSgf() const override;
    void loadSgfText(const QString &sgf, const QString &title) override;
    bool startTsumegoSet(const QString &key, const QString &name, const QString &sgf) override;
    int tsumegoSolved(const QString &key, int total) const override;
    void startAiGameAt(int level) override;
    void fillStrengthChoices(QComboBox *box) const override;
    QString strengthLabel(int level) const override;

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void buildUi();
    void newGame();
    void newAiGame();
    void swapSides();
    void setAiVisits(int v);
    bool ensureHumanModel(int v);
    void maybeAiMove();
    void sendAiQuery();
    void sendSideQuery(int purpose, int visits);
    void backfillGraph();
    void countScore();
    void showCount(GameNode *node, const QJsonObject &resp);
    void checkModel(bool manual);
    void autosave();
    void restoreAutosave();
    void onModelList(const QList<NetworkInfo> &list);
    void openSgf();
    bool loadSgfContent(const QString &text, const QString &displayName, const QString &path);
    void saveSgf();
    bool editGameInfo();
    void openSettings();
    void restartEngine();
    void appendLog(const QString &line);
    void showAbout();
    bool chooseGpu();
    void bootstrap();    // 第一次使用：缺引擎先下载引擎，缺模型再下载模型，齐了就启动
    void installEngine(const EngineBuild &b, const QString &why);
    void showDownloadProgress(const QString &what, const QString &text, int percent, const QString &title = {});
    void chooseModel();
    void openModelChooser(const QList<NetworkInfo> &list);
    void startEngine();  // 先识别硬件、选好引擎和显卡，再启动
    void onPointClicked(int x, int y);
    void onEngineState(KataGoEngine::State s, const QString &msg);
    void onAnalysis(const QJsonObject &resp);
    void positionChanged();
    void requestAnalysis();
    void refreshInfo();
    void refreshReference();
    QJsonObject buildQuery() const;
    QJsonObject buildQueryFor(const QList<GameNode *> &path) const;
    // 复盘：整盘分析、胜率曲线、坏棋
    void startReview();
    void refreshReview();
    void jumpToMistake(int dir);
    // 死活题练习（MainWindowTsumego.cpp）
    void enterTsumego();
    void exitTsumego();
    void openTsCollection();
    void openTsEditor();
    void loadTsProblem(int i);
    QJsonObject tsQuery(Stone player) const;
    void tsSendBest();
    void tsOnClick(int x, int y);
    bool tsHandleResponse(const QJsonObject &resp);
    void tsAfterNavigate();
    void tsUndo();
    void tsShowAnswer();
    void tsRefresh();
    QString tsProgressGroup() const;
    static QString tsProgressGroupFor(const QString &key);
    bool useTsProblems(const QList<TsumegoProblem> &list, const QString &key, const QString &name);
    // 在线：菜单、登录状态
    void buildOnlineMenu();
    void addOnlineToolbar(class QToolBar *tb);
    void refreshOnline();
    void onlineLogin();
    // 对局室（MainWindowRoom.cpp）
    void openRooms();
    void enterRoom(qint64 id);
    void leaveRoom();
    void applyRoomBoard(const QString &sgf, const QString &cursor);
    void pushRoomBoard();
    QString roomCursor() const;
    bool roomBlocksAi() const;
    bool roomAllowsClick();
    bool askRoomResult(QString *result);
    void clearQueries();  // 换棋盘内容前清掉还指向旧节点的引擎查询
    class RoomSession *m_room = nullptr;
    class QDockWidget *m_roomDock = nullptr;
    class RoomPanel *m_roomPanel = nullptr;
    bool m_applyingRoom = false;
    QAction *m_roomsAct = nullptr;
    QMenu *m_onlineMenu = nullptr;
    QAction *m_loginAct = nullptr, *m_tbLoginAct = nullptr, *m_pwAct = nullptr, *m_cloudAct = nullptr, *m_hwAct = nullptr, *m_adminAct = nullptr;
    QLabel *m_onlineStatus = nullptr;
    enum class TsState { Thinking, UserTurn, Replying, Solved, Failed };
    bool m_tsMode = false;
    QList<TsumegoProblem> m_tsProblems;
    QString m_tsKey, m_tsName, m_tsMessage;
    int m_tsIndex = 0;
    Stone m_tsPlayer = Stone::Black;
    QRect m_tsRect;
    QJsonArray m_tsAllow, m_tsBestOwn;
    TsState m_tsState = TsState::Thinking;
    QString m_tsBestId, m_tsReplyId, m_tsBestMove, m_tsUserMove;
    QStringList m_tsBestPv;
    bool m_tsShowAnswerPending = false;
    bool m_tsWasAnalyzing = false, m_tsWasCandidates = true;
    QWidget *m_reviewPanel = nullptr, *m_tsPanel = nullptr;
    QLabel *m_tsTitle = nullptr, *m_tsStatus = nullptr;
    QAction *m_tsAct = nullptr;
    class QSplitter *m_leftSplit = nullptr;
    class WinrateGraph *m_graph = nullptr;
    QLabel *m_reviewInfo = nullptr;
    class QPushButton *m_reviewBtn = nullptr;
    QComboBox *m_reviewVisits = nullptr;
    QString m_reviewQueryId;
    QString m_fillQueryId;            // 后台补算胜率曲线缺的那几手
    QList<GameNode *> m_fillLine;
    int m_fillLeft = 0;
    QList<GameNode *> m_reviewLine;   // 第 i 个是第 i 手之后的局面（0 = 开局）
    int m_reviewDone = 0;
    QList<MoveEval> m_evals;

    AppSettings m_settings;
    GameTree m_tree;
    KataGoEngine *m_engine;
    BoardWidget *m_board;
    QLabel *m_gameInfo, *m_analysisInfo, *m_engineStatus;
    QPlainTextEdit *m_log, *m_comment;
    QListWidget *m_refList;
    QLabel *m_lastMoveRef;
    QString m_pinnedMove;
    QProgressDialog *m_dlProgress = nullptr;
    QProgressDialog *m_prepProgress = nullptr; // 第一次调优 / 测试硬件的进度  // 右侧选中的参考图第一手
    QAction *m_analyzeAct, *m_candAct, *m_ownAct;
    QString m_queryId;
    GameNode *m_queryNode = nullptr;
    QString m_sgfPath;
    ModelUpdater *m_updater;
    EngineInstaller *m_installer;
    bool m_manualCheck = false;
    bool m_choosingModel = false;       // 正在为“选择模型”读取官方列表
    bool m_manualModelDownload = false; // 用户手动选的模型正在下载（显示进度框）
    QList<NetworkInfo> m_officialModels;
    QAction *m_autoUpdAct = nullptr;
    bool m_firstModel = false;  // 第一次使用、还没有模型：自动下载
    QStringList m_triedBuilds;
    QString m_chosenGpuName;    // 自动选中的显卡名字，用来核对 KataGo 实际用的是不是它
    int m_gpuOverride = -1;     // 核对后修正的 OpenCL 编号  // 本次已经试过、起不来的引擎
    // 人机对弈
    Stone m_aiColor = Stone::Empty;
    int m_aiVisits = 400;        // 正数 = 每步计算量；负数 = 模仿人类段位
    int m_pendingStrength = 0;   // 等人类风格模型下载/加载好后要切换到的强度
    bool m_humanDownload = false;
    QComboBox *m_strengthBox = nullptr;
    QString m_aiQueryId;
    GameNode *m_aiNode = nullptr;
    bool m_aiPaused = false;
    bool m_aiOneShot = false;
    // 额外请求：0 = 只为显示形势，1 = 点目
    struct SideQuery { GameNode *node; int purpose; };
    QHash<QString, SideQuery> m_side;
    QString m_ownQueryId;  // “让 AI 走这一步”：替当前一方走一步  // 浏览/悔棋后 AI 暂停，等你落子再继续
};
