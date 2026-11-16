#include "RoomUi.h"
#include <QPoint>
#include "ApiClient.h"
#include "OnlineDialogs.h"
#include "RoomSession.h"
#include "../I18n.h"
#include "../Rules.h"
#include "../AppSettings.h"
#include <QDoubleSpinBox>
#include <QButtonGroup>
#include <QTimer>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
ApiClient *api() { return ApiClient::instance(); }
qint64 idOf(const QJsonObject &o, const char *key = "id") { return qint64(o.value(key).toDouble()); }

QString roomKindName(const QJsonObject &r)
{
    if (r["match"].toBool()) return T("比赛");
    return r["kind"].toString() == QLatin1String("lecture") ? T("班级大课") : T("指导棋");
}

QString roomWho(const QJsonObject &r)
{
    if (r["kind"].toString() == QLatin1String("lecture")) return r["class_name"].toString();
    QString s = T("黑：%1　白：%2").arg(r["black_name"].toString(), r["white_name"].toString());
    if (!r["class_name"].toString().isEmpty()) s += T("（%1 观摩）").arg(r["class_name"].toString());
    return s;
}
}

// ---- 对局室列表 ----

RoomsDialog::RoomsDialog(OnlineHost *host, QWidget *parent) : QDialog(parent), m_host(host)
{
    setWindowTitle(T("对局室"));
    auto *lay = new QVBoxLayout(this);
    auto *hint = new QLabel(api()->isTeacher() ? T("开一个指导棋或班级大课，学生登录后在这里就能看到并进入。")
                                               : T("老师开的指导棋和大课会出现在这里，双击进入。"), this);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({T("名称"), T("类型"), T("老师"), T("对局双方 / 班级")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_table, 1);
    auto *btns = new QHBoxLayout;
    auto *enterBtn = new QPushButton(T("进入"), this);
    enterBtn->setDefault(true);
    btns->addWidget(enterBtn);
    if (api()->isTeacher()) {
        auto *gameBtn = new QPushButton(T("开指导棋…"), this);
        auto *matchBtn = new QPushButton(T("开比赛…"), this);
        auto *lectureBtn = new QPushButton(T("开班级大课…"), this);
        btns->addWidget(gameBtn);
        btns->addWidget(matchBtn);
        btns->addWidget(lectureBtn);
        connect(matchBtn, &QPushButton::clicked, this, [this] { create("match"); });
        connect(gameBtn, &QPushButton::clicked, this, [this] { create("game"); });
        connect(lectureBtn, &QPushButton::clicked, this, [this] { create("lecture"); });
    }
    auto *refreshBtn = new QPushButton(T("刷新"), this);
    auto *closeBtn = new QPushButton(T("关闭"), this);
    btns->addWidget(refreshBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    lay->addLayout(btns);
    connect(enterBtn, &QPushButton::clicked, this, &RoomsDialog::enterSelected);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &RoomsDialog::enterSelected);
    connect(refreshBtn, &QPushButton::clicked, this, &RoomsDialog::reload);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    resize(720, 400);
    reload();
}

void RoomsDialog::reload()
{
    QPointer<RoomsDialog> self(this);
    api()->get("/rooms", [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("对局室"), r); return; }
        self->m_rooms = r.data.toArray();
        self->m_table->setRowCount(int(self->m_rooms.size()));
        for (int i = 0; i < self->m_rooms.size(); ++i) {
            const QJsonObject o = self->m_rooms[i].toObject();
            const QString title = o["title"].toString().isEmpty() ? roomKindName(o) : o["title"].toString();
            const QStringList cells{title, roomKindName(o), o["owner_name"].toString(), roomWho(o)};
            for (int c = 0; c < cells.size(); ++c) self->m_table->setItem(i, c, new QTableWidgetItem(cells[c]));
        }
        if (!self->m_rooms.isEmpty()) self->m_table->selectRow(0);
    });
}

void RoomsDialog::enterSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_rooms.size()) return;
    m_chosen = idOf(m_rooms[row].toObject());
    accept();
}

void RoomsDialog::create(const QString &kind)
{
    CreateRoomDialog dlg(m_host, kind, this);
    if (dlg.exec() != QDialog::Accepted || !dlg.createdRoom()) return;
    m_chosen = dlg.createdRoom();
    accept();
}

// ---- 开房 ----

CreateRoomDialog::CreateRoomDialog(OnlineHost *host, const QString &kind, QWidget *parent)
    : QDialog(parent), m_host(host), m_kind(kind)
{
    const bool lecture = kind == QLatin1String("lecture");
    const bool match = kind == QLatin1String("match");
    setWindowTitle(lecture ? T("开班级大课") : match ? T("开比赛") : T("开指导棋"));
    auto *form = new QFormLayout(this);
    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(T("可以不填"));
    form->addRow(T("名称："), m_title);
    if (match) {
        m_black = new QComboBox(this);
        m_white = new QComboBox(this);
        form->addRow(T("执黑："), m_black);
        form->addRow(T("执白："), m_white);
    } else if (!lecture) {
        m_student = new QComboBox(this);
        form->addRow(T("学生："), m_student);
        m_color = new QComboBox(this);
        m_color->addItem(T("老师执白，学生执黑"), "W");
        m_color->addItem(T("老师执黑，学生执白"), "B");
        form->addRow(T("执子："), m_color);
    }
    m_rules = new QComboBox(this);
    AppSettings prefs;
    prefs.load();
    Rules::fillBox(m_rules, prefs.rules);
    m_komi = new QDoubleSpinBox(this);
    m_komi->setRange(-50, 50);
    m_komi->setSingleStep(0.5);
    m_komi->setSuffix(T(" 目"));
    m_komi->setValue(Rules::get(m_rules->currentData().toString()).komi);
    connect(m_rules, &QComboBox::currentIndexChanged, this, [this] {
        if (m_handicap && m_handicap->value() >= 2) return;  // 让子棋贴目固定 0.5
        m_komi->setValue(Rules::get(m_rules->currentData().toString()).komi);
    });
    form->addRow(T("规则："), m_rules);
    if (!lecture) {
        m_handicap = new QSpinBox(this);
        m_handicap->setRange(0, 9);
        m_handicap->setSpecialValueText(T("不让子"));
        m_handicap->setSuffix(T(" 子"));
        m_handicap->setToolTip(T("让子时黑方先摆子，白方先走，贴目自动改为 0.5"));
        form->addRow(T("让子："), m_handicap);
        connect(m_handicap, &QSpinBox::valueChanged, this, [this](int h) {
            if (h == 1) { m_handicap->setValue(m_handicap->property("last").toInt() >= 2 ? 0 : 2); return; }  // 没有让 1 子
            m_handicap->setProperty("last", h);
            m_komi->setValue(h >= 2 ? 0.5 : Rules::get(m_rules->currentData().toString()).komi);
            if (h >= 2 && m_color) m_color->setCurrentIndex(0);  // 让子棋老师执白
            if (m_color) m_color->setEnabled(h < 2);
        });
    }
    form->addRow(T("贴目："), m_komi);
    m_class = new QComboBox(this);
    if (!lecture) m_class->addItem(T("不需要"), 0);
    form->addRow(lecture ? T("班级：") : T("全班观摩："), m_class);
    if (!lecture) {
        // 计时：比赛默认计时，指导棋默认不计时
        m_useClock = new QCheckBox(T("计时"), this);
        m_useClock->setChecked(match);
        m_mainMin = new QSpinBox(this);
        m_mainMin->setRange(0, 360);
        m_mainMin->setValue(match ? 20 : 10);
        m_mainMin->setSuffix(T(" 分钟"));
        m_periods = new QSpinBox(this);
        m_periods->setRange(0, 20);
        m_periods->setValue(3);
        m_periods->setSuffix(T(" 次"));
        m_periodSec = new QSpinBox(this);
        m_periodSec->setRange(5, 600);
        m_periodSec->setValue(30);
        m_periodSec->setSuffix(T(" 秒"));
        auto *clockRow = new QHBoxLayout;
        clockRow->addWidget(m_useClock);
        clockRow->addWidget(new QLabel(T("基本用时"), this));
        clockRow->addWidget(m_mainMin);
        clockRow->addWidget(new QLabel(T("读秒"), this));
        clockRow->addWidget(m_periods);
        clockRow->addWidget(m_periodSec);
        clockRow->addStretch();
        form->addRow(T("用时："), clockRow);
        auto syncClock = [this] {
            for (QWidget *w : {static_cast<QWidget *>(m_mainMin), static_cast<QWidget *>(m_periods), static_cast<QWidget *>(m_periodSec)})
                w->setEnabled(m_useClock->isChecked());
        };
        connect(m_useClock, &QCheckBox::toggled, this, syncClock);
        syncClock();
    }
    auto *startRow = new QHBoxLayout;
    auto *empty = new QRadioButton(T("空棋盘"), this);
    m_fromBoard = new QRadioButton(T("当前棋盘上的棋局"), this);
    empty->setChecked(true);
    auto *group = new QButtonGroup(this);
    group->addButton(empty);
    group->addButton(m_fromBoard);
    startRow->addWidget(empty);
    startRow->addWidget(m_fromBoard);
    startRow->addStretch();
    form->addRow(T("开始局面："), startRow);
    if (m_handicap) connect(m_fromBoard, &QRadioButton::toggled, m_handicap, [this](bool on) {
        if (on) m_handicap->setValue(0);
        m_handicap->setEnabled(!on);
    });
    auto *note = new QLabel(lecture ? T("开房后你摆的每一步全班都能实时看到；可以请某个学生上台摆棋。结束时棋谱存进你的云棋谱。")
                            : match ? T("两个学生对下，你当裁判：可以悔棋、结束并判输赢。超时自动判负。结束时棋谱存进你和双方的云棋谱。")
                                    : T("开房后学生在“对局室”里就能进入。结束时棋谱存进你和学生的云棋谱。"), this);
    note->setWordWrap(true);
    note->setStyleSheet("color: gray;");
    form->addRow(note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = bb->button(QDialogButtonBox::Ok);
    m_ok->setText(T("开房"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    connect(bb, &QDialogButtonBox::accepted, this, &CreateRoomDialog::submit);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(bb);

    QPointer<CreateRoomDialog> self(this);
    api()->get("/classes", [self](const ApiResult &r) {
        if (!self || !r.ok) return;
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject c = v.toObject();
            self->m_class->addItem(T("%1（%2 人）").arg(c["name"].toString()).arg(c["students"].toInt()), idOf(c));
        }
    });
    if (m_student || m_black) {
        api()->get("/users", [self](const ApiResult &r) {
            if (!self || !r.ok) return;
            for (const QJsonValue &v : r.data.toArray()) {
                const QJsonObject u = v.toObject();
                if (u["role"].toString() != QLatin1String("student") || u["status"].toString() == QLatin1String("disabled")) continue;
                QString text = T("%1（%2）").arg(u["display_name"].toString(), u["username"].toString());
                if (u["rank"].toInt() != 0) text += "  " + Online::rankName(u["rank"].toInt());
                for (QComboBox *box : {self->m_student, self->m_black, self->m_white})
                    if (box) box->addItem(text, idOf(u));
            }
            if (self->m_white && self->m_white->count() > 1) self->m_white->setCurrentIndex(1);
        });
    }
    resize(460, sizeHint().height());
}

void CreateRoomDialog::submit()
{
    QJsonObject body{{"kind", m_kind}, {"title", m_title->text().trimmed()}};
    const qint64 classId = m_class->currentData().toLongLong();
    if (m_kind == QLatin1String("lecture")) {
        if (!classId) { QMessageBox::warning(this, windowTitle(), T("请先选择班级。")); return; }
        body["class_id"] = classId;
    } else if (m_kind == QLatin1String("match")) {
        if (m_black->currentIndex() < 0 || m_white->currentIndex() < 0 || m_black->currentData() == m_white->currentData()) {
            QMessageBox::warning(this, windowTitle(), T("请选择两个不同的学生。"));
            return;
        }
        body["black_id"] = m_black->currentData().toLongLong();
        body["white_id"] = m_white->currentData().toLongLong();
        if (classId) body["class_id"] = classId;
    } else {
        if (m_student->currentIndex() < 0) { QMessageBox::warning(this, windowTitle(), T("请先选择学生。")); return; }
        body["student_id"] = m_student->currentData().toLongLong();
        body["teacher_color"] = m_color->currentData().toString();
        if (classId) body["class_id"] = classId;
    }
    if (m_useClock && m_useClock->isChecked()) {
        const int periods = m_periods->value();
        body["clock"] = QJsonObject{{"main_sec", m_mainMin->value() * 60}, {"periods", periods},
                                    {"period_sec", periods > 0 ? m_periodSec->value() : 0}};
        if (m_mainMin->value() == 0 && periods == 0) { QMessageBox::warning(this, windowTitle(), T("计时至少要有基本用时或读秒。")); return; }
    }
    if (m_fromBoard->isChecked()) {
        body["sgf"] = m_host->currentGameSgf();
    } else {
        const Rules::Info &ru = Rules::get(m_rules->currentData().toString());
        QString sgf = QStringLiteral("(;GM[1]FF[4]CA[UTF-8]SZ[19]RU[%1]KM[%2]").arg(ru.sgf).arg(m_komi->value());
        const int h = m_handicap ? m_handicap->value() : 0;
        if (h >= 2) {
            // 19 路标准让子位置（和人机对弈一样），白先走
            QList<QPoint> pts = {{15, 3}, {3, 15}};
            if (h >= 3) pts.append({15, 15});
            if (h >= 4) pts.append({3, 3});
            if (h == 5 || h == 7 || h == 9) pts.append({9, 9});
            if (h >= 6) { pts.append({3, 9}); pts.append({15, 9}); }
            if (h >= 8) { pts.append({9, 3}); pts.append({9, 15}); }
            sgf += QStringLiteral("HA[%1]AB").arg(h);
            for (const QPoint &p : pts) sgf += QStringLiteral("[%1%2]").arg(QChar('a' + p.x())).arg(QChar('a' + p.y()));
            sgf += QStringLiteral("PL[W]");
        }
        body["sgf"] = sgf + ")";
    }
    m_ok->setEnabled(false);
    QPointer<CreateRoomDialog> self(this);
    api()->post("/rooms", body, [self](const ApiResult &r) {
        if (!self) return;
        self->m_ok->setEnabled(true);
        if (!r.ok) { Online::showError(self, self->windowTitle(), r); return; }
        self->m_created = idOf(r.data.toObject());
        self->accept();
    });
}

// ---- 房间面板 ----

RoomPanel::RoomPanel(RoomSession *session, QWidget *parent) : QWidget(parent), m_s(session)
{
    auto *lay = new QVBoxLayout(this);
    m_title = new QLabel(this);
    m_title->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_title->setWordWrap(true);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    lay->addWidget(m_title);
    lay->addWidget(m_status);
    if (!m_s->clock().isEmpty()) {
        m_clockLabel = new QLabel(this);
        m_clockLabel->setStyleSheet("font-size: 15px; font-family: monospace;");
        lay->addWidget(m_clockLabel);
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &RoomPanel::tickClock);
        timer->start(250);
    }
    lay->addWidget(new QLabel(T("在房间里："), this));
    m_online = new QListWidget(this);
    m_online->setMaximumHeight(130);
    lay->addWidget(m_online);
    if (m_s->canManage() && m_s->isLecture()) {
        auto *row = new QHBoxLayout;
        m_handTo = new QComboBox(this);
        m_handBtn = new QPushButton(T("请他上台"), this);
        m_handBtn->setToolTip(T("把摆棋权交给这个学生，他摆的棋全班都能看到"));
        row->addWidget(m_handTo, 1);
        row->addWidget(m_handBtn);
        lay->addLayout(row);
        m_takeBtn = new QPushButton(T("收回摆棋权"), this);
        lay->addWidget(m_takeBtn);
        connect(m_handBtn, &QPushButton::clicked, this, [this] {
            if (m_handTo->currentIndex() >= 0) m_s->setControl(m_handTo->currentData().toLongLong());
        });
        connect(m_takeBtn, &QPushButton::clicked, this, [this] { m_s->setControl(0); });
    }
    lay->addWidget(new QLabel(T("聊天："), this));
    m_chat = new QTextBrowser(this);
    lay->addWidget(m_chat, 1);
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(T("输入后按回车发送"));
    m_input->setMaxLength(500);
    lay->addWidget(m_input);
    connect(m_input, &QLineEdit::returnPressed, this, &RoomPanel::sendChat);
    auto *btns = new QHBoxLayout;
    if (m_s->canManage()) {
        m_closeBtn = new QPushButton(T("结束并保存棋谱"), this);
        btns->addWidget(m_closeBtn);
        connect(m_closeBtn, &QPushButton::clicked, this, &RoomPanel::closeRequested);
    }
    auto *leaveBtn = new QPushButton(T("离开"), this);
    btns->addWidget(leaveBtn);
    lay->addLayout(btns);
    connect(leaveBtn, &QPushButton::clicked, this, &RoomPanel::leaveRequested);
    refresh();
}

void RoomPanel::refresh()
{
    const QJsonObject room = m_s->room();
    m_title->setText(m_s->title());
    QSet<qint64> online;
    for (const QJsonValue &v : m_s->online()) online.insert(idOf(v.toObject()));
    // 房主 + 被邀请的人，在线的排前面
    struct Person { qint64 id; QString name; };
    QList<Person> people{{idOf(room, "owner_id"), room["owner_name"].toString()}};
    for (const QJsonValue &v : room["members"].toArray()) {
        const QJsonObject u = v.toObject();
        if (idOf(u) != people.first().id) people << Person{idOf(u), u["name"].toString()};
    }
    std::stable_sort(people.begin(), people.end(), [&online](const Person &a, const Person &b) {
        return online.contains(a.id) && !online.contains(b.id);
    });
    m_online->clear();
    for (const Person &p : people) {
        QString label = p.name;
        if (p.id == idOf(room, "owner_id")) label += T("（老师）");
        if (m_s->isLecture() && p.id == m_s->controlId() && p.id != idOf(room, "owner_id")) label += T("（正在摆棋）");
        auto *it = new QListWidgetItem((online.contains(p.id) ? QStringLiteral("● ") : QStringLiteral("○ ")) + label, m_online);
        if (!online.contains(p.id)) it->setForeground(Qt::gray);
    }
    if (m_handTo) {
        const qint64 keep = m_handTo->currentData().toLongLong();
        m_handTo->clear();
        for (const Person &p : people)
            if (p.id != idOf(room, "owner_id")) m_handTo->addItem(p.name, p.id);
        const int i = m_handTo->findData(keep);
        if (i >= 0) m_handTo->setCurrentIndex(i);
        m_takeBtn->setEnabled(m_s->controlId() != idOf(room, "owner_id"));
    }

    // 现在的状态：轮到谁、我能不能动
    QString status;
    if (m_s->isLecture()) {
        const qint64 c = m_s->controlId();
        QString who = room["owner_name"].toString();
        for (const QJsonValue &v : room["members"].toArray())
            if (idOf(v.toObject()) == c) who = v.toObject()["name"].toString();
        status = c == api()->myId() ? T("你在摆棋，全班都能看到。") : T("%1 正在摆棋，你的棋盘会跟着变。").arg(who);
    } else {
        const QString color = m_s->toMove() == QLatin1String("W") ? T("白") : T("黑");
        const QString mine = m_s->myColor();
        if (mine.isEmpty()) status = T("轮到%1走。").arg(color);
        else if (mine == m_s->toMove()) status = T("轮到你走（%1）。").arg(color);
        else status = T("等对方走（你执%1）。").arg(mine == QLatin1String("W") ? T("白") : T("黑"));
        if (m_s->canManage()) status += m_s->isMatch() ? T("你是裁判，可以悔棋、结束并判输赢。") : T("老师可以随时悔棋、摆变化。");
    }
    m_status->setText(status);
}

// 钟：服务器给的是轮到的一方已用多久，这里按本机时间接着往下倒数
void RoomPanel::tickClock()
{
    const QJsonObject c = m_s->clock();
    if (!m_clockLabel || c.isEmpty()) return;
    const qint64 periodMs = qint64(c["period_sec"].toInt()) * 1000;
    const bool running = c["running"].toBool();
    const qint64 extra = running ? qint64(c["elapsed_ms"].toDouble()) + QDateTime::currentMSecsSinceEpoch() - m_s->clockReceivedMs() : 0;
    auto show = [&](const QString &color, const char *msKey, const char *pKey) {
        qint64 main = qint64(c[msKey].toDouble());
        int periods = c[pKey].toInt();
        qint64 inPeriod = periodMs;
        if (running && c["turn"].toString() == color) {
            qint64 used = extra;
            if (used <= main) main -= used;
            else {
                used -= main;
                main = 0;
                while (periodMs > 0 && used > periodMs && periods > 0) { --periods; used -= periodMs; }
                inPeriod = qMax<qint64>(0, periodMs - used);
            }
        }
        auto mmss = [](qint64 ms) { const qint64 s = (ms + 999) / 1000; return QStringLiteral("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0')); };
        QString text = main > 0 ? mmss(main) + (periods > 0 ? QStringLiteral(" + %1×%2").arg(periods).arg(periodMs / 1000) : QString())
                                : periods > 0 ? T("读秒 %1（剩 %2 次）").arg(mmss(inPeriod)).arg(periods) : QStringLiteral("00:00");
        const bool moving = running && c["turn"].toString() == color;
        return QStringLiteral("%1%2 %3").arg(moving ? QStringLiteral("▶ ") : QStringLiteral("　"), color == "B" ? T("黑") : T("白"), text);
    };
    QString text = show("B", "black_ms", "black_periods") + "\n" + show("W", "white_ms", "white_periods");
    if (!running && !m_s->result().isEmpty()) text += "\n" + T("结果：%1").arg(m_s->result());
    else if (!running) text += "\n" + T("第一手落下后开始计时。");
    m_clockLabel->setText(text);
}

void RoomPanel::addChat(const QJsonArray &messages)
{
    for (const QJsonValue &v : messages) {
        const QJsonObject m = v.toObject();
        const QString time = QDateTime::fromSecsSinceEpoch(qint64(m["at"].toDouble())).toString("HH:mm");
        m_chat->append(QStringLiteral("<span style='color:gray'>%1</span> <b>%2</b>：%3")
                           .arg(time, m["name"].toString().toHtmlEscaped(), m["text"].toString().toHtmlEscaped()));
    }
}

void RoomPanel::note(const QString &text)
{
    m_chat->append(QStringLiteral("<span style='color:gray'>%1</span>").arg(text.toHtmlEscaped()));
}

void RoomPanel::sendChat()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_s->sendChat(text);
    m_input->clear();
}
