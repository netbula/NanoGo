#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class OnlineHost;
class RoomSession;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QTableWidget;
class QTextBrowser;

// 对局室列表：进入别人开的房间；老师还可以开指导棋或班级大课
class RoomsDialog : public QDialog {
    Q_OBJECT
public:
    RoomsDialog(OnlineHost *host, QWidget *parent = nullptr);
    qint64 chosenRoom() const { return m_chosen; }

private:
    void reload();
    void enterSelected();
    void create(const QString &kind);
    OnlineHost *m_host;
    QTableWidget *m_table;
    QJsonArray m_rooms;
    qint64 m_chosen = 0;
};

// 开房：指导棋（选学生、老师执黑或执白、可选全班观摩）或班级大课（选班级）
class CreateRoomDialog : public QDialog {
    Q_OBJECT
public:
    CreateRoomDialog(OnlineHost *host, const QString &kind, QWidget *parent = nullptr);
    qint64 createdRoom() const { return m_created; }

private:
    void submit();
    OnlineHost *m_host;
    QString m_kind;
    QLineEdit *m_title;
    QComboBox *m_student = nullptr, *m_color = nullptr, *m_class, *m_rules;
    QComboBox *m_black = nullptr, *m_white = nullptr;  // 比赛的双方
    class QDoubleSpinBox *m_komi;
    class QCheckBox *m_useClock = nullptr;
    class QSpinBox *m_mainMin = nullptr, *m_periods = nullptr, *m_periodSec = nullptr;
    QRadioButton *m_fromBoard;
    QPushButton *m_ok;
    qint64 m_created = 0;
};

// 进房后停靠在主窗口右边：谁在线、轮到谁、聊天，老师还有交出摆棋权和结束
class RoomPanel : public QWidget {
    Q_OBJECT
public:
    RoomPanel(RoomSession *session, QWidget *parent = nullptr);
    void refresh();
    void addChat(const QJsonArray &messages);
    void note(const QString &text);  // 灰色的系统提示

signals:
    void leaveRequested();
    void closeRequested();

private:
    void sendChat();
    void tickClock();
    RoomSession *m_s;
    QLabel *m_title, *m_status;
    QLabel *m_clockLabel = nullptr;
    QListWidget *m_online;
    QTextBrowser *m_chat;
    QLineEdit *m_input;
    QComboBox *m_handTo = nullptr;
    QPushButton *m_handBtn = nullptr, *m_takeBtn = nullptr, *m_closeBtn = nullptr;
};
