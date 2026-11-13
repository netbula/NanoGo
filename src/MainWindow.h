#pragma once
#include "AppSettings.h"
#include "GameTree.h"
#include "KataGoEngine.h"
#include "ModelUpdater.h"
#include <QMainWindow>

class BoardWidget;
class QLabel;
class QPlainTextEdit;
class QAction;

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
    void maybeAiMove();
    void checkModel(bool manual);
    void onModelList(const QList<NetworkInfo> &list);
    void openSgf();
    void saveSgf();
    void openSettings();
    void restartEngine();
    void onPointClicked(int x, int y);
    void onEngineState(KataGoEngine::State s, const QString &msg);
    void onAnalysis(const QJsonObject &resp);
    void positionChanged();
    void requestAnalysis();
    void refreshInfo();
    QJsonObject buildQuery() const;

    AppSettings m_settings;
    GameTree m_tree;
    KataGoEngine *m_engine;
    BoardWidget *m_board;
    QLabel *m_gameInfo, *m_analysisInfo, *m_engineStatus;
    QPlainTextEdit *m_log, *m_comment;
    QAction *m_analyzeAct, *m_candAct, *m_ownAct;
    QString m_queryId;
    GameNode *m_queryNode = nullptr;
    QString m_sgfPath;
    ModelUpdater *m_updater;
    bool m_manualCheck = false;
    // 人机对弈
    Stone m_aiColor = Stone::Empty;
    int m_aiVisits = 400;
    QString m_aiQueryId;
    GameNode *m_aiNode = nullptr;
};
