#include "ApiClient.h"
#include "../AppSettings.h"
#include "../I18n.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

QString ApiResult::message() const
{
    if (status == 0) return T("连不上服务器，请检查网络和服务器地址。");
    static const QHash<QString, const char *> texts = {
        {"bad_credentials", "用户名或密码不对。"},
        {"too_many_attempts", "登录失败次数太多，请 5 分钟后再试。"},
        {"account_disabled", "这个账号已被停用，请联系管理员。"},
        {"unauthorized", "登录已过期，请重新登录。"},
        {"forbidden", "没有权限做这件事。"},
        {"not_found", "找不到，可能已被删除。"},
        {"weak_password", "密码至少要 6 位。"},
        {"bad_username", "用户名只能用 3 到 32 位小写字母、数字和 _ . -"},
        {"username_taken", "这个用户名已经有人用了。"},
        {"guardian_consent_required", "未成年学生需要先取得监护人同意。"},
        {"cannot_change_self", "不能停用自己或改自己的角色。"},
        {"not_a_teacher", "任课老师必须是老师或管理员账号。"},
        {"not_a_student", "只能把学生账号加进班级。"},
        {"class_empty", "这个班还没有学生。"},
        {"not_in_class", "有学生不在这个班里。"},
        {"bad_sgf", "不是有效的 SGF 棋谱。"},
        {"bad_rank", "棋力不在可选范围内。"},
        {"class_not_found", "找不到目标班级，可能已被删除。"},
    };
    if (texts.contains(code)) return T(texts.value(code));
    const QString detail = data.toObject().value("message").toString();
    return T("服务器出错（%1）").arg(detail.isEmpty() ? QString::number(status) : detail);
}

ApiClient *ApiClient::instance()
{
    static ApiClient *c = new ApiClient;
    return c;
}

ApiClient::ApiClient(QObject *parent) : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

QString ApiClient::normalizeUrl(QString url)
{
    url = url.trimmed();
    while (url.endsWith('/')) url.chop(1);
    if (!url.isEmpty() && !url.contains("://")) {
        // 没写协议：本机和局域网地址用 http，其余用 https
        const bool local = url.startsWith("localhost") || url.startsWith("127.") || url.startsWith("192.168.") || url.startsWith("10.");
        url = (local ? "http://" : "https://") + url;
    }
    return url;
}

QString ApiClient::roleName(const QString &role)
{
    if (role == QLatin1String("admin")) return T("管理员");
    if (role == QLatin1String("teacher")) return T("老师");
    return T("学生");
}

void ApiClient::sendRaw(const QString &base, const QString &token, const QByteArray &method, const QString &path,
                        const QJsonValue &body, Callback cb)
{
    QNetworkRequest req(QUrl(base + "/api/v1" + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "NanoGo");
    if (!token.isEmpty()) req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    req.setTransferTimeout(30000);
    QByteArray data;
    if (!body.isUndefined() && !body.isNull())
        data = body.isArray() ? QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact)
                              : QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_net->sendCustomRequest(req, method, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, token] {
        reply->deleteLater();
        ApiResult r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        r.data = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
        r.ok = r.status >= 200 && r.status < 300;
        if (!r.ok) r.code = doc.object().value("error").toString();
        // 令牌失效（被停用、改了密码、过期）：退出登录
        if (r.status == 401 && !token.isEmpty() && token == m_token) dropSession();
        if (cb) cb(r);
    });
}

void ApiClient::send(const QByteArray &method, const QString &path, const QJsonValue &body, Callback cb)
{
    if (m_base.isEmpty() || m_token.isEmpty()) {
        ApiResult r;
        r.status = 401;
        r.code = "unauthorized";
        if (cb) cb(r);
        return;
    }
    sendRaw(m_base, m_token, method, path, body, cb);
}

void ApiClient::login(const QString &baseUrl, const QString &user, const QString &password, bool remember, Callback cb)
{
    const QString base = normalizeUrl(baseUrl);
    const QString name = user.trimmed().toLower();
    sendRaw(base, QString(), "POST", "/auth/login", QJsonObject{{"username", name}, {"password", password}},
            [this, base, name, remember, cb](const ApiResult &r) {
                if (r.ok) {
                    m_base = base;
                    m_user = name;
                    m_token = r.data.toObject().value("token").toString();
                    m_me = r.data.toObject().value("user").toObject();
                    if (remember) Credentials::save(base, name, m_token);
                    else Credentials::remove(base, name);
                    emit sessionChanged();
                }
                if (cb) cb(r);
            });
}

void ApiClient::restore(const QString &baseUrl, const QString &user)
{
    const QString base = normalizeUrl(baseUrl);
    if (base.isEmpty() || user.isEmpty()) return;
    const QString token = Credentials::load(base, user);
    if (token.isEmpty()) return;
    sendRaw(base, token, "GET", "/me", QJsonValue(), [this, base, user, token](const ApiResult &r) {
        if (r.ok) {
            m_base = base;
            m_user = user;
            m_token = token;
            m_me = r.data.toObject();
            emit sessionChanged();
        } else if (r.status == 401) {
            Credentials::remove(base, user);
        }
    });
}

void ApiClient::logout()
{
    if (!m_token.isEmpty()) sendRaw(m_base, m_token, "POST", "/auth/logout", QJsonValue(), nullptr);
    Credentials::remove(m_base, m_user);
    dropSession();
}

void ApiClient::dropSession()
{
    const bool was = !m_token.isEmpty();
    m_token.clear();
    m_me = QJsonObject();
    if (was) emit sessionChanged();
}

// ---- 凭据 ----

namespace {
QString credTarget(const QString &server, const QString &user) { return "NanoGo:" + user + "@" + server; }
}

#ifdef Q_OS_WIN
void Credentials::save(const QString &server, const QString &user, const QString &token)
{
    const std::wstring target = credTarget(server, user).toStdWString();
    const std::wstring name = user.toStdWString();
    QByteArray blob = token.toUtf8();
    CREDENTIALW c = {};
    c.Type = CRED_TYPE_GENERIC;
    c.TargetName = const_cast<LPWSTR>(target.c_str());
    c.UserName = const_cast<LPWSTR>(name.c_str());
    c.CredentialBlobSize = DWORD(blob.size());
    c.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&c, 0);
}

QString Credentials::load(const QString &server, const QString &user)
{
    const std::wstring target = credTarget(server, user).toStdWString();
    PCREDENTIALW c = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &c)) return {};
    const QString token = QString::fromUtf8(reinterpret_cast<const char *>(c->CredentialBlob), int(c->CredentialBlobSize));
    CredFree(c);
    return token;
}

void Credentials::remove(const QString &server, const QString &user)
{
    const std::wstring target = credTarget(server, user).toStdWString();
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}
#else
// 其他系统：存在数据目录的 credentials.ini（仅开发用）
namespace {
QSettings &credStore()
{
    static QSettings s(QDir(AppSettings::dataDir()).filePath("credentials.ini"), QSettings::IniFormat);
    return s;
}
}
void Credentials::save(const QString &server, const QString &user, const QString &token)
{
    credStore().setValue(QString::fromLatin1(credTarget(server, user).toUtf8().toHex()), token);
}
QString Credentials::load(const QString &server, const QString &user)
{
    return credStore().value(QString::fromLatin1(credTarget(server, user).toUtf8().toHex())).toString();
}
void Credentials::remove(const QString &server, const QString &user)
{
    credStore().remove(QString::fromLatin1(credTarget(server, user).toUtf8().toHex()));
}
#endif
