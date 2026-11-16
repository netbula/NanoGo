#include "RoomSession.h"
#include "ApiClient.h"
#include "../I18n.h"
#include <QPointer>
#include <QTimer>

namespace {
ApiClient *api() { return ApiClient::instance(); }
qint64 num(const QJsonValue &v) { return qint64(v.toDouble()); }
}

RoomSession::RoomSession(qint64 roomId, QObject *parent) : QObject(parent), m_id(roomId) {}

RoomSession::~RoomSession() { stop(); }

QString RoomSession::title() const
{
    const QString t = m_room.value("title").toString();
    if (!t.isEmpty()) return t;
    return isLecture() ? T("%1 大课").arg(m_room.value("class_name").toString())
                       : T("指导棋：%1 对 %2").arg(m_room.value("black_name").toString(), m_room.value("white_name").toString());
}

bool RoomSession::canManage() const
{
    return api()->isAdmin() || num(m_room.value("owner_id")) == api()->myId();
}

QString RoomSession::myColor() const
{
    if (isLecture()) return {};
    const qint64 me = api()->myId();
    if (num(m_room.value("black_id")) == me) return QStringLiteral("B");
    if (num(m_room.value("white_id")) == me) return QStringLiteral("W");
    return {};
}

bool RoomSession::canEditNow() const
{
    if (m_closed) return false;
    if (canManage()) return true;
    if (isLecture()) return m_controlId == api()->myId();
    return !myColor().isEmpty() && myColor() == m_toMove;
}

void RoomSession::start()
{
    m_running = true;
    ++m_generation;
    fetchFull();
}

void RoomSession::stop()
{
    m_running = false;
    ++m_generation;
}

void RoomSession::fetchFull()
{
    const int gen = m_generation;
    QPointer<RoomSession> self(this);
    api()->get(QStringLiteral("/rooms/%1").arg(m_id), [self, gen](const ApiResult &r) {
        if (!self || gen != self->m_generation || !self->m_running) return;
        if (!r.ok) {
            if (r.status == 404 || r.status == 401) { emit self->problem(r.message()); emit self->ended(false); return; }
            QTimer::singleShot(3000, self, [self, gen] { if (self && gen == self->m_generation) self->fetchFull(); });
            return;
        }
        const QJsonObject st = r.data.toObject();
        const bool first = self->m_room.isEmpty();
        self->m_room = st.value("room").toObject();
        if (first) emit self->ready();
        self->handleState(st, true);
        self->poll();
    });
}

void RoomSession::poll()
{
    if (!m_running || m_closed) return;
    const int gen = m_generation;
    QPointer<RoomSession> self(this);
    api()->get(QStringLiteral("/rooms/%1/poll?since=%2").arg(m_id).arg(m_seq), [self, gen](const ApiResult &r) {
        if (!self || gen != self->m_generation || !self->m_running) return;
        if (!r.ok) {
            if (r.status == 404 || r.status == 401) { emit self->problem(r.message()); emit self->ended(false); return; }
            // 网络断了：越等越久地重试，不打扰
            const int delay = qMin(30000, 1000 << qMin(self->m_failures++, 5));
            if (self->m_failures == 3) emit self->problem(T("和服务器的连接断了，正在重连…"));
            QTimer::singleShot(delay, self, [self, gen] { if (self && gen == self->m_generation) self->poll(); });
            return;
        }
        if (self->m_failures >= 3) emit self->problem(T("已重新连上服务器。"));
        self->m_failures = 0;
        self->handleState(r.data.toObject(), false);
        self->poll();
    });
}

void RoomSession::handleState(const QJsonObject &st, bool force)
{
    m_seq = qMax(m_seq, num(st.value("seq")));
    const qint64 version = num(st.value("version"));
    // 只换上比手里更新的棋盘；自己刚发上去的那一版不用重新装
    if (force || version > m_version) {
        const QString sgf = st.value("sgf").toString(), cursor = st.value("cursor").toString();
        const bool differs = sgf != m_sgf || cursor != m_cursor;
        m_version = version;
        m_sgf = sgf;
        m_cursor = cursor;
        if ((differs || force) && m_pending.isEmpty()) emit boardChanged(sgf, cursor);
    }
    m_toMove = st.value("to_move").toString(m_toMove);
    m_controlId = num(st.value("control_id"));
    m_online = st.value("online").toArray();
    const QJsonArray chat = st.value("chat").toArray();
    if (!chat.isEmpty()) emit chatArrived(chat);
    emit statusChanged();
    if (st.value("closed").toBool() && !m_closed) {
        m_closed = true;
        m_running = false;
        emit ended(m_closing);
    }
}

void RoomSession::pushState(const QString &sgf, const QString &cursor, const QString &toMove)
{
    if (m_closed) return;
    if (sgf == m_sgf && cursor == m_cursor && toMove == m_toMove && m_pending.isEmpty() && !m_sending) return;
    m_pending = {Pending{sgf, cursor, toMove}};  // 只留最新的
    if (!m_sending) sendNext();
}

void RoomSession::sendNext()
{
    if (m_pending.isEmpty() || !m_running) { m_sending = false; return; }
    const Pending p = m_pending.takeFirst();
    m_sending = true;
    QPointer<RoomSession> self(this);
    const QJsonObject body{{"sgf", p.sgf}, {"cursor", p.cursor}, {"to_move", p.toMove}, {"base_version", m_version}};
    api()->put(QStringLiteral("/rooms/%1/state").arg(m_id), body, [self, p](const ApiResult &r) {
        if (!self) return;
        self->m_sending = false;
        if (r.ok) {
            self->m_version = qMax(self->m_version, num(r.data.toObject().value("version")));
            self->m_sgf = p.sgf;
            self->m_cursor = p.cursor;
            self->m_toMove = p.toMove;
            emit self->statusChanged();
            self->sendNext();
            return;
        }
        // 别人刚改过、不轮到我、房间已结束：丢掉本地改动，换回服务器上的棋盘
        self->m_pending.clear();
        emit self->problem(r.message());
        if (self->m_running) { ++self->m_generation; self->fetchFull(); }  // 旧的轮询作废，重新开始
    });
}

void RoomSession::sendChat(const QString &text)
{
    QPointer<RoomSession> self(this);
    api()->post(QStringLiteral("/rooms/%1/chat").arg(m_id), QJsonObject{{"text", text}}, [self](const ApiResult &r) {
        if (self && !r.ok) emit self->problem(r.message());
    });
}

void RoomSession::setControl(qint64 userId)
{
    QPointer<RoomSession> self(this);
    api()->put(QStringLiteral("/rooms/%1/control").arg(m_id), QJsonObject{{"user_id", userId}}, [self](const ApiResult &r) {
        if (self && !r.ok) emit self->problem(r.message());
    });
}

void RoomSession::closeRoom(const QString &result)
{
    m_closing = true;
    QPointer<RoomSession> self(this);
    api()->post(QStringLiteral("/rooms/%1/close").arg(m_id), QJsonObject{{"result", result}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok && r.code != QLatin1String("room_closed")) {
            self->m_closing = false;
            emit self->problem(r.message());
            return;
        }
        if (!self->m_closed) {
            self->m_closed = true;
            self->m_running = false;
            emit self->ended(true);
        }
    });
}
