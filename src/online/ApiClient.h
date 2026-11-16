#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <functional>

class QNetworkAccessManager;

// 教学服务端的接口客户端。不登录时什么都不做，NanoGo 照常作为单机版使用。
struct ApiResult {
    bool ok = false;
    int status = 0;     // HTTP 状态码，0 = 连不上
    QString code;       // 服务端的错误代码，例如 bad_credentials
    QJsonValue data;    // 成功时的返回内容
    QString message() const; // 给用户看的出错说明（当前界面语言）
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const ApiResult &)>;

    static ApiClient *instance();

    QString baseUrl() const { return m_base; }
    bool loggedIn() const { return !m_token.isEmpty() && !m_me.isEmpty(); }
    QJsonObject me() const { return m_me; }
    qint64 myId() const { return qint64(m_me.value("id").toDouble()); }
    QString role() const { return m_me.value("role").toString(); }
    bool isAdmin() const { return role() == QLatin1String("admin"); }
    bool isTeacher() const { return role() == QLatin1String("teacher") || isAdmin(); }
    bool isStudent() const { return role() == QLatin1String("student"); }

    // 登录；remember 时把令牌存进系统凭据管理器，下次启动自动登录
    void login(const QString &baseUrl, const QString &user, const QString &password, bool remember, Callback cb);
    // 启动时用保存的令牌恢复登录（没有或已过期就保持未登录）
    void restore(const QString &baseUrl, const QString &user);
    void logout();

    // 通用请求。path 例如 "/games"，自动加 /api/v1 前缀和令牌
    void get(const QString &path, Callback cb) { send("GET", path, QJsonValue(), cb); }
    void post(const QString &path, const QJsonValue &body, Callback cb) { send("POST", path, body, cb); }
    void put(const QString &path, const QJsonValue &body, Callback cb) { send("PUT", path, body, cb); }
    void patch(const QString &path, const QJsonValue &body, Callback cb) { send("PATCH", path, body, cb); }
    void del(const QString &path, Callback cb) { send("DELETE", path, QJsonValue(), cb); }
    void send(const QByteArray &method, const QString &path, const QJsonValue &body, Callback cb);

    static QString roleName(const QString &role);
    static QString normalizeUrl(QString url);

signals:
    void sessionChanged(); // 登录、退出、会话失效

private:
    explicit ApiClient(QObject *parent = nullptr);
    void sendRaw(const QString &base, const QString &token, const QByteArray &method, const QString &path,
                 const QJsonValue &body, Callback cb);
    void dropSession();

    QNetworkAccessManager *m_net;
    QString m_base, m_token, m_user;
    QJsonObject m_me;
};

// 令牌保存在系统凭据管理器（Windows 凭据管理器），不写进 nanogo.ini
namespace Credentials {
void save(const QString &server, const QString &user, const QString &token);
QString load(const QString &server, const QString &user);
void remove(const QString &server, const QString &user);
}
