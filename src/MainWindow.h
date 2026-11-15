#pragma once
#include "AppSettings.h"
#include "GameTree.h"
#include "KataGoEngine.h"
#include "ModelUpdater.h"
#include "EngineInstaller.h"
#include <QHash>
#include <QMainWindow>

class BoardWidget;
class QLabel;
class QPlainTextEdit;
class QAction;
class QListWidget;
class QProgressDialog;
class QComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void buildUi();
    void newGame();
    void newAiGame();
    void swapSides();
    void setAiVisits(int v);
    void maybeAiMove();
    void sendAiQuery();
    void sendSideQuery(int purpose, int visits);
    void countScore();
    void showCount(GameNode *node, const QJsonObject &resp);
    void checkModel(bool manual);
    void autosave();
    void restoreAutosave();
    void onModelList(const QList<NetworkInfo> &list);
    void openSgf();
    void saveSgf();
    void openSettings();
    void restartEngine();
    void appendLog(const QString &line);
    void bootstrap();    // 第一次使用：缺引擎先下载引擎，缺模型再下载模型，齐了就启动
    void installEngine(const EngineBuild &b, const QString &why);
    void showDownloadProgress(const QString &what, const QString &text, int percent);
    void startEngine();  // 先识别硬件、选好引擎和显卡，再启动
    void onPointClicked(int x, int y);
    void onEngineState(KataGoEngine::State s, const QString &msg);
    void onAnalysis(const QJsonObject &resp);
    void positionChanged();
    void requestAnalysis();
    void refreshInfo();
    void refreshReference();
    QJsonObject buildQuery() const;

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
    bool m_firstModel = false;  // 第一次使用、还没有模型：自动下载
    QStringList m_triedBuilds;
    QString m_chosenGpuName;    // 自动选中的显卡名字，用来核对 KataGo 实际用的是不是它
    int m_gpuOverride = -1;     // 核对后修正的 OpenCL 编号  // 本次已经试过、起不来的引擎
    // 人机对弈
    Stone m_aiColor = Stone::Empty;
    int m_aiVisits = 400;
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
