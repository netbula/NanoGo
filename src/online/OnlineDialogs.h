#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QTableWidget;
class QTextBrowser;
class QListWidget;
class QPushButton;
struct ApiResult;

// 在线功能需要主窗口提供的几件事（MainWindow 实现）
class OnlineHost {
public:
    virtual ~OnlineHost() = default;
    virtual QString currentGameSgf() const = 0;
    virtual void loadSgfText(const QString &sgf, const QString &title) = 0;
    // 用一组死活题进入做题模式；key 决定做题进度存在哪里
    virtual bool startTsumegoSet(const QString &key, const QString &name, const QString &sgf) = 0;
    virtual int tsumegoSolved(const QString &key, int total) const = 0;
    virtual void startAiGameAt(int level) = 0;
    virtual void fillStrengthChoices(QComboBox *box) const = 0;
    virtual QString strengthLabel(int level) const = 0;
};

namespace Online {
void showError(QWidget *parent, const QString &title, const ApiResult &r);
QString formatTime(qint64 unixSecs);
QString homeworkKindName(const QString &kind);
// 作业的做题进度键：区分服务器和账号，同一台电脑上换账号不会串
QString homeworkKey(qint64 homeworkId);
void changePassword(QWidget *parent);
// 介绍免费的 NanoGo 教学服务端，带下载按钮（推广用，不登录也能打开）
void showServerInfo(QWidget *parent);
extern const char *const kServerDownloadUrl;
// 棋力：0 = 未填；-k = k 级；-(100+d) = 业余 d 段；-(200+d) = 职业 d 段（与服务端一致）
QString rankName(int rank);
void fillRankBox(QComboBox *box, int current);
// 老师查看和设置自己班学生的棋力
void editClassRanks(QWidget *parent, qint64 classId, const QString &className);
}

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    LoginDialog(const QString &server, const QString &user, QWidget *parent = nullptr);
    QString server() const;
    QString user() const;

signals:
    // 每次点“登录”都发出，不管成功与否：主窗口借此记住服务器地址和用户名
    void attempted(const QString &server, const QString &user);

private:
    void tryLogin();
    QLineEdit *m_server, *m_user, *m_password;
    QCheckBox *m_remember;
    QLabel *m_error;
    QPushButton *m_ok;
};

class CloudGamesDialog : public QDialog {
    Q_OBJECT
public:
    CloudGamesDialog(OnlineHost *host, QWidget *parent = nullptr);

private:
    void reload();
    void openSelected();
    void upload();
    void uploadFiles();
    void renameSelected();
    void deleteSelected();
    qint64 selectedId() const;
    OnlineHost *m_host;
    QComboBox *m_owner = nullptr;
    QTableWidget *m_table;
    QPushButton *m_uploadBtn, *m_uploadFilesBtn, *m_renameBtn, *m_deleteBtn;
    QJsonArray m_games;
};

class HomeworkDialog : public QDialog {
    Q_OBJECT
public:
    HomeworkDialog(OnlineHost *host, QWidget *parent = nullptr);

private:
    void reloadClasses();
    void reload();
    void showDetail();
    QJsonObject selected() const;
    // 学生
    void startSelected();
    void submitSelected();
    void submitWithGames(const QJsonObject &hw, const QJsonArray &gameIds, const QString &note);
    // 老师
    void assign();
    void viewSubmissions();
    void deleteSelected();

    OnlineHost *m_host;
    QPushButton *m_ranksBtn = nullptr;
    bool m_teacher;
    QComboBox *m_class = nullptr;
    QTableWidget *m_table;
    QTextBrowser *m_detail;
    QPushButton *m_startBtn = nullptr, *m_submitBtn = nullptr;
    QPushButton *m_assignBtn = nullptr, *m_subsBtn = nullptr, *m_deleteBtn = nullptr;
    QJsonArray m_list;
};

class AssignHomeworkDialog : public QDialog {
    Q_OBJECT
public:
    AssignHomeworkDialog(OnlineHost *host, qint64 classId, QWidget *parent = nullptr);

private:
    void kindChanged();
    void loadProblemSets(qint64 select = 0);
    void browseProblems();
    void browseGame();
    void submit();
    OnlineHost *m_host;
    qint64 m_classId;
    QLineEdit *m_title;
    QComboBox *m_kind, *m_problemSet, *m_strength;
    class QSpinBox *m_aiGames;
    class QPlainTextEdit *m_desc;
    class QDateTimeEdit *m_due;
    QCheckBox *m_hasDue;
    QListWidget *m_students;
    QWidget *m_tsRow, *m_reviewRow, *m_aiRow;
    QLabel *m_gameLabel;
    qint64 m_gameId = 0;     // 选了云棋谱
    QString m_gameSgf;       // 选了本地棋谱；两者都空 = 当前棋盘
    QPushButton *m_ok;
};

class SubmissionsDialog : public QDialog {
    Q_OBJECT
public:
    SubmissionsDialog(OnlineHost *host, const QJsonObject &homework, QWidget *parent = nullptr);

private:
    void reload();
    void openGame();
    void review();
    QJsonObject selectedSubmission() const;
    OnlineHost *m_host;
    QJsonObject m_hw;
    QTableWidget *m_table;
    QJsonArray m_rows;
};

class AdminDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdminDialog(QWidget *parent = nullptr);

private:
    QWidget *buildUsersTab();
    QWidget *buildClassesTab();
    QWidget *buildAuditTab();
    void reloadUsers();
    void editUser(bool create);
    void toggleUser();
    void resetPassword();
    void reloadClasses();
    void reloadMembers();
    void editClass(bool create);
    void deleteClass();
    void addMember();
    void removeMember();
    void moveMember();
    void reloadAudit();
    QJsonObject selectedUser() const;
    QJsonObject selectedClass() const;

    QTableWidget *m_users, *m_audit;
    QListWidget *m_classes, *m_members;
    QJsonArray m_userList, m_classList, m_memberList;
};
