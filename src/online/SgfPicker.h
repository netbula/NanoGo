#pragma once
#include "../GameTree.h"
#include "../Tsumego.h"
#include <QDialog>

class BoardWidget;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;
class OnlineHost;

// 布置作业时浏览、挑选死活题：服务器上的题集、自带题、本地文件或文件夹，带棋盘预览，可以只勾其中几道
class ProblemPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProblemPickerDialog(QWidget *parent = nullptr);
    QList<TsumegoProblem> problems() const;  // 勾选的题
    QString collectionName() const { return m_name; }
    // 选的是服务器上的题集且全部勾选时，给出它的编号（不用再上传）；否则 0
    qint64 serverSetId() const;
    bool allChecked() const;

private:
    void addFileSource(const QString &path, bool select);
    void openFile();
    void openFolder();
    void sourceChanged();
    void setProblems(const QList<TsumegoProblem> &list, const QString &name);
    void showPreview();
    void updateCount();

    QComboBox *m_source;
    QListWidget *m_list;
    QLabel *m_info, *m_count;
    GameTree m_tree;
    BoardWidget *m_board;
    QList<TsumegoProblem> m_problems;
    QString m_name;
    qint64 m_setId = 0;
    int m_loadSerial = 0;
    class QPushButton *m_ok;
};

// 布置复盘作业时浏览、挑选棋谱：当前棋盘、云棋谱（自己或学生的）、本地文件或文件夹，带棋盘预览
class GamePickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit GamePickerDialog(OnlineHost *host, QWidget *parent = nullptr);
    qint64 gameId() const { return m_gameId; }  // 选的是云棋谱时的编号，否则 0
    QString sgf() const { return m_sgf; }        // 选的是本地棋谱或当前棋盘时的内容
    QString title() const { return m_title; }

private:
    void addLocalSource(const QString &path, bool select);
    void openFile();
    void openFolder();
    void sourceChanged();
    void selectionChanged();
    void showSgf(const QString &sgf);
    void showMove(int n);

    OnlineHost *m_host;
    QComboBox *m_source;
    QListWidget *m_list;
    QLabel *m_info;
    QSlider *m_slider;
    GameTree m_tree;
    BoardWidget *m_board;
    qint64 m_gameId = 0;
    QString m_sgf, m_title;
    int m_loadSerial = 0;
    class QPushButton *m_ok;
};
