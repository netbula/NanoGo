#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

// 一个对局室的联网部分：长轮询等别人的变化，把自己的改动发上去。不碰界面。
class RoomSession : public QObject {
    Q_OBJECT
public:
    RoomSession(qint64 roomId, QObject *parent = nullptr);
    ~RoomSession() override;
    void start();
    void stop();

    qint64 id() const { return m_id; }
    QJsonObject room() const { return m_room; }
    QString kind() const { return m_room.value("kind").toString(); }
    bool isLecture() const { return kind() == QLatin1String("lecture"); }
    bool isMatch() const { return m_room.value("match").toBool(); }
    QJsonObject clock() const { return m_clock; }   // 空 = 不计时；见服务端 clockView
    qint64 clockReceivedMs() const { return m_clockAt; } // 收到 clock 时的本机时间（毫秒）
    QString result() const { return m_result; }
    QString title() const;
    QString toMove() const { return m_toMove; }
    qint64 controlId() const { return m_controlId; }
    QJsonArray online() const { return m_online; }
    QString cursor() const { return m_cursor; }
    bool canManage() const;          // 房主或管理员
    QString myColor() const;         // 指导棋里我执的颜色 B / W，不下棋的是空
    bool canEditNow() const;         // 现在我能不能改棋盘

    // 把自己的棋盘发上去（只发最新的一份，发送中再改会排队）
    void pushState(const QString &sgf, const QString &cursor, const QString &toMove);
    void sendChat(const QString &text);
    void reportAi(const QStringList &programs);  // 防作弊：告诉老师这台电脑上在运行的围棋 AI
    void setControl(qint64 userId);
    void closeRoom(const QString &result = QString());  // result：B+R、W+3.5、0（和棋），空 = 没下完

signals:
    void ready();                                              // 房间信息读到了
    void boardChanged(const QString &sgf, const QString &cursor); // 别人改了棋盘，要换上
    void chatArrived(const QJsonArray &messages);
    void statusChanged();                                       // 轮到谁、谁在线、谁有摆棋权
    void ended(bool byMe);                                      // 房间结束了
    void problem(const QString &message);                       // 出错提示（显示在状态栏）

private:
    void poll();
    void fetchFull();
    void handleState(const QJsonObject &st, bool force);
    void sendNext();

    qint64 m_id;
    QJsonObject m_room;
    qint64 m_seq = 0, m_version = -1, m_controlId = 0;
    QString m_toMove = QStringLiteral("B"), m_sgf, m_cursor;
    QJsonArray m_online;
    QJsonObject m_clock;
    qint64 m_clockAt = 0;
    QString m_result;
    bool m_running = false, m_closed = false, m_sending = false, m_closing = false;
    int m_generation = 0;   // stop/start 之后丢掉旧的轮询结果
    int m_failures = 0;
    struct Pending { QString sgf, cursor, toMove; };
    QList<Pending> m_pending;
};
