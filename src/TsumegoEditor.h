#pragma once
#include "GameTree.h"
#include "Tsumego.h"
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>

class BoardWidget;
class KataGoEngine;
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QRadioButton;

// 死活题编辑器：摆黑白子、设谁先走和目标、整理题目顺序，存成 SGF 题库；可以让 KataGo 检查题目成不成立
class TsumegoEditor : public QDialog {
    Q_OBJECT
public:
    TsumegoEditor(KataGoEngine *engine, QWidget *parent = nullptr);
    void setCollection(const QList<TsumegoProblem> &list, const QString &path);
    QList<TsumegoProblem> problems() const { return m_list; }
    QString filePath() const { return m_path; }
    void reject() override;  // 关闭前问要不要保存

signals:
    // 想马上做这一套题（从第 index 题开始）
    void practiceRequested(const QList<TsumegoProblem> &list, const QString &path, int index);


public slots:
    void check();

private:
    void refreshList();
    void showProblem(int i);
    void syncBoard();
    void edit(int x, int y, bool erase);
    void newProblem();
    void duplicateProblem();
    void deleteProblem();
    void moveProblem(int dir);
    void openFile();
    bool save();
    bool saveAs();
    bool maybeSave();
    void onAnalysis(const QJsonObject &resp);
    TsumegoProblem &cur() { return m_list[m_index]; }

    KataGoEngine *m_engine;
    GameTree m_tree;
    BoardWidget *m_board;
    QListWidget *m_listW;
    QLineEdit *m_title;
    QComboBox *m_toPlay, *m_goal;
    QRadioButton *m_toolBlack, *m_toolWhite, *m_toolErase, *m_toolAlt;
    QLabel *m_checkInfo, *m_fileInfo;
    QList<TsumegoProblem> m_list;
    int m_index = 0;
    QString m_path;
    bool m_dirty = false;
    bool m_altNext = true;  // “黑白交替”工具下一颗是黑
    // KataGo 检查：先找最佳着法（带归属），再看脱先时对方应过之后的归属
    QString m_bestId, m_tenukiId;
    QJsonObject m_bestResp, m_tenukiResp;
    QJsonArray m_allow;
    QRect m_rect;
};
