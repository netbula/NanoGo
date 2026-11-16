// 对局室（二期）：指导棋和班级大课。进房后主棋盘和服务器同步，离开后恢复原来的棋局。
#include "MainWindow.h"
#include "BoardWidget.h"
#include "I18n.h"
#include "Sgf.h"
#include "online/AiGuard.h"
#include "online/ApiClient.h"
#include "online/RoomSession.h"
#include "online/RoomUi.h"
#include <QAction>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDockWidget>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QSet>
#include <QJsonArray>
#include <QApplication>

void MainWindow::openRooms()
{
    if (!ApiClient::instance()->loggedIn()) return;
    RoomsDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted && dlg.chosenRoom()) {
        if (m_room && !ApiClient::instance()->isStudent()) switchRoom(dlg.chosenRoom());  // 老师再开一盘：前一盘在后台继续
        else enterRoom(dlg.chosenRoom());
    }
}

bool MainWindow::roomBlocksAi() const
{
    // 学生在对局室里不能用 AI 提示（下指导棋、上大课时都一样）
    return m_room && ApiClient::instance()->isStudent();
}

// 当前所在节点：从根往下每一步选第几个分支，点分隔
QString MainWindow::roomCursor() const
{
    QStringList path;
    for (GameNode *n : m_tree.pathToCurrent())
        if (n->parent) path << QString::number(n->parent->children.indexOf(n));
    return path.join('.');
}

void MainWindow::pushRoomBoard()
{
    if (!m_room || !m_room->canEditNow()) return;
    m_room->pushState(Sgf::save(m_tree), roomCursor(), m_tree.toPlay() == Stone::White ? "W" : "B");
}

void MainWindow::enterRoom(qint64 id)
{
    if (m_room) leaveRoom();
    if (ApiClient::instance()->isStudent()) {
        // 防作弊：学生进房前先关掉自己的 KataGo，再查电脑上有没有别的围棋 AI
        m_engine->stop();
        m_roomStoppedEngine = true;
        const QStringList found = AiGuard::findGoAi();
        if (!found.isEmpty()) {
            QMessageBox::warning(this, T("对局室"), T("请先关闭下面这些程序，再进入对局室：\n%1").arg(found.join('\n')));
            m_roomStoppedEngine = false;
            restartEngine();
            return;
        }
    }
    if (m_tsMode) exitTsumego();
    if (!m_switchingRoom) autosave();  // 进房前的棋局存好，离开时恢复（在对局之间切换时不动）
    m_aiColor = Stone::Empty;
    m_aiQueryId.clear();
    m_aiNode = nullptr;
    m_room = new RoomSession(id, this);
    connect(m_room, &RoomSession::ready, this, [this] {
        m_roomPanel = new RoomPanel(m_room, this);
        m_roomDock = new QDockWidget(T("对局室"), this);
        m_roomDock->setObjectName("roomDock");
        m_roomDock->setFeatures(QDockWidget::DockWidgetMovable);
        m_roomDock->setWidget(m_roomPanel);
        addDockWidget(Qt::RightDockWidgetArea, m_roomDock);
        connect(m_roomPanel, &RoomPanel::leaveRequested, this, &MainWindow::leaveRoom);
        connect(m_roomPanel, &RoomPanel::closeRequested, this, [this] {
            QString result;
            if (!askRoomResult(&result)) return;
            m_room->closeRoom(result);
        });
        if (roomBlocksAi()) {
            m_analyzeAct->setChecked(false);
            m_analyzeAct->setEnabled(false);
        }
        setWindowTitle(T("NanoGo 围棋 - 对局室：%1").arg(m_room->title()));
        m_roomPanel->note(T("已进入对局室。"));
        connect(m_roomPanel, &RoomPanel::switchRequested, this, &MainWindow::switchRoom);
        if (m_room->canManage() && !m_room->isLecture()) {
            if (!m_simulTimer) {
                m_simulTimer = new QTimer(this);
                connect(m_simulTimer, &QTimer::timeout, this, &MainWindow::refreshSimul);
            }
            m_simulTimer->start(15000);
            refreshSimul();
        }
        if (ApiClient::instance()->isStudent()) {
            m_roomPanel->note(T("对局期间 NanoGo 会检查这台电脑上有没有运行其他围棋 AI，发现了会告诉老师。"));
            m_aiFound.clear();
            if (!m_aiGuard) {
                m_aiGuard = new QTimer(this);
                connect(m_aiGuard, &QTimer::timeout, this, &MainWindow::checkRoomAi);
            }
            m_aiGuard->start(8000);
        }
    });
    connect(m_room, &RoomSession::boardChanged, this, &MainWindow::applyRoomBoard);
    connect(m_room, &RoomSession::chatArrived, this, [this](const QJsonArray &msgs) {
        if (m_roomPanel) m_roomPanel->addChat(msgs);
    });
    connect(m_room, &RoomSession::statusChanged, this, [this] {
        if (m_roomPanel) m_roomPanel->refresh();
    });
    connect(m_room, &RoomSession::problem, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 5000);
        if (m_roomPanel) m_roomPanel->note(msg);
    });
    connect(m_room, &RoomSession::ended, this, [this](bool byMe) {
        const bool game = m_room && !m_room->isLecture();
        byMe = byMe || (m_room && m_room->canManage());  // 老师在别处（另一台电脑）结束的也算自己结束
        QTimer::singleShot(0, this, &MainWindow::leaveRoom);
        const QString result = m_room ? m_room->result() : QString();
        QString text = byMe ? T("对局室已结束，棋谱已存进你的云棋谱。")
                     : game ? T("老师结束了对局室，棋谱已存进你的云棋谱。") : T("老师结束了对局室。");
        if (!result.isEmpty()) text = T("对局结束：%1").arg(resultText(result)) + "\n" + text;
        QMessageBox::information(this, T("对局室"), text);
    });
    m_room->start();
}

// 把 SGF 的 RE 写成中文：B+R → 黑中盘胜，W+T → 白超时胜，B+3.5 → 黑胜 3.5 目
QString MainWindow::resultText(const QString &re)
{
    if (re == QLatin1String("0") || re.startsWith(QLatin1String("Draw"), Qt::CaseInsensitive)) return T("和棋");
    const QString who = re.startsWith('B') ? T("黑") : re.startsWith('W') ? T("白") : QString();
    if (who.isEmpty()) return re;
    const QString how = re.mid(2);
    if (how.startsWith('R')) return T("%1中盘胜").arg(who);
    if (how.startsWith('T')) return T("%1超时胜").arg(who);
    if (how.startsWith('F')) return T("%1判胜").arg(who);
    return how.isEmpty() ? T("%1胜").arg(who) : T("%1胜 %2 目").arg(who, how);
}

// 结束对局室前确认；指导棋顺便选输赢，存进棋谱（双方名字、日期开房时已自动写好）
bool MainWindow::askRoomResult(QString *result)
{
    QDialog dlg(this);
    dlg.setWindowTitle(T("结束对局室"));
    auto *form = new QFormLayout(&dlg);
    auto *note = new QLabel(T("结束后棋谱存进云棋谱，其他人会自动退出。"), &dlg);
    note->setWordWrap(true);
    form->addRow(note);
    QComboBox *kind = nullptr;
    QDoubleSpinBox *margin = nullptr;
    if (!m_room->isLecture()) {
        kind = new QComboBox(&dlg);
        kind->addItem(T("没下完"), "");
        kind->addItem(T("黑中盘胜"), "B+R");
        kind->addItem(T("白中盘胜"), "W+R");
        kind->addItem(T("黑胜（目数）"), "B+");
        kind->addItem(T("白胜（目数）"), "W+");
        kind->addItem(T("黑超时胜"), "B+T");
        kind->addItem(T("白超时胜"), "W+T");
        kind->addItem(T("和棋"), "0");
        margin = new QDoubleSpinBox(&dlg);
        margin->setRange(0.5, 361);
        margin->setSingleStep(1);
        margin->setDecimals(1);
        margin->setValue(0.5);
        margin->setSuffix(T(" 目"));
        margin->setEnabled(false);
        form->addRow(T("结果："), kind);
        form->addRow(T("胜几目："), margin);
        connect(kind, &QComboBox::currentIndexChanged, margin, [kind, margin] {
            margin->setEnabled(kind->currentData().toString().endsWith('+'));
        });
    }
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(T("结束并保存"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return false;
    result->clear();
    if (kind) {
        *result = kind->currentData().toString();
        if (result->endsWith('+')) *result += QString::number(margin->value());
    }
    return true;
}

void MainWindow::leaveRoom()
{
    if (!m_room) return;
    if (m_aiGuard) m_aiGuard->stop();
    if (!m_switchingRoom) {
        if (m_simulTimer) m_simulTimer->stop();
        for (RoomSession *w : std::as_const(m_watch)) { w->stop(); w->deleteLater(); }
        m_watch.clear();
    }
    m_room->stop();
    m_room->deleteLater();
    m_room = nullptr;
    if (m_roomDock) {
        removeDockWidget(m_roomDock);
        m_roomDock->deleteLater();
    }
    m_roomDock = nullptr;
    m_roomPanel = nullptr;
    m_analyzeAct->setEnabled(true);
    m_engine->terminateAll();
    clearQueries();
    setWindowTitle(T("NanoGo 围棋"));
    if (m_switchingRoom) return;  // 马上进下一盘
    restoreAutosave();
    if (m_roomStoppedEngine) {  // 学生离开对局室：AI 恢复
        m_roomStoppedEngine = false;
        restartEngine();
    }
    positionChanged();
}

// 老师同时下多盘：切到另一盘，当前这盘转到后台
void MainWindow::switchRoom(qint64 id)
{
    if (m_room && m_room->id() == id) return;
    m_switchingRoom = true;
    if (RoomSession *w = m_watch.take(id)) { w->stop(); w->deleteLater(); }
    leaveRoom();
    enterRoom(id);
    m_switchingRoom = false;
    QTimer::singleShot(0, this, &MainWindow::refreshSimul);
}

// 找出老师自己开着的其他指导棋，在后台跟着
void MainWindow::refreshSimul()
{
    if (!m_room || !m_room->canManage() || m_room->isLecture()) return;
    const qint64 me = ApiClient::instance()->myId();
    ApiClient::instance()->get(QStringLiteral("/rooms"), [this, me](const ApiResult &r) {
        if (!r.ok || !m_room) return;
        QSet<qint64> open;
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject o = v.toObject();
            const qint64 id = qint64(o["id"].toDouble());
            if (qint64(o["owner_id"].toDouble()) != me || o["kind"].toString() != QLatin1String("game") || o["match"].toBool()
                || o["status"].toString(QStringLiteral("open")) != QLatin1String("open") || id == m_room->id())
                continue;
            open.insert(id);
            if (m_watch.contains(id)) continue;
            auto *w = new RoomSession(id, this);
            m_watch.insert(id, w);
            connect(w, &RoomSession::statusChanged, this, [this, w] {
                const bool myTurn = !w->myColor().isEmpty() && w->myColor() == w->toMove();
                if (myTurn && !w->property("myTurn").toBool()) {
                    statusBar()->showMessage(T("%1：轮到你走了。").arg(w->title()), 8000);
                    QApplication::alert(this);
                }
                w->setProperty("myTurn", myTurn);
                updateSimulList();
            });
            connect(w, &RoomSession::ended, this, [this, id] {
                if (RoomSession *x = m_watch.take(id)) { x->stop(); x->deleteLater(); }
                updateSimulList();
            });
            w->start();
        }
        for (auto it = m_watch.begin(); it != m_watch.end();) {
            if (!open.contains(it.key())) { it.value()->stop(); it.value()->deleteLater(); it = m_watch.erase(it); }
            else ++it;
        }
        updateSimulList();
    });
}

void MainWindow::updateSimulList()
{
    if (!m_roomPanel) return;
    QList<RoomPanel::OtherGame> games;
    for (RoomSession *w : std::as_const(m_watch)) {
        if (w->room().isEmpty()) continue;  // 还没读到房间信息
        games << RoomPanel::OtherGame{w->id(), w->title(), !w->myColor().isEmpty() && w->myColor() == w->toMove()};
    }
    m_roomPanel->setOtherGames(games);
}

// 防作弊：定时查一次；有变化（新发现或都关掉了）就报告给老师
void MainWindow::checkRoomAi()
{
    if (!m_room) return;
    const QStringList found = AiGuard::findGoAi();
    if (found == m_aiFound) return;
    m_aiFound = found;
    m_room->reportAi(found);
    if (!found.isEmpty()) {
        const QString msg = T("检测到正在运行：%1。对局期间请关闭，老师已经收到提醒。").arg(found.join(QStringLiteral("、")));
        if (m_roomPanel) m_roomPanel->note(msg);
        QMessageBox::warning(this, T("对局室"), msg);
    }
}

void MainWindow::applyRoomBoard(const QString &sgf, const QString &cursor)
{
    m_engine->terminateAll();
    clearQueries();
    QString err;
    if (!Sgf::load(sgf, m_tree, &err)) {
        statusBar()->showMessage(err, 5000);
        return;
    }
    GameNode *n = m_tree.root();
    for (const QString &part : cursor.split('.', Qt::SkipEmptyParts)) {
        const int i = part.toInt();
        if (i < 0 || i >= n->children.size()) break;
        n = n->children[i];
    }
    m_applyingRoom = true;
    m_tree.setCurrent(n);
    positionChanged();
    m_applyingRoom = false;
}

// 落子前问一下：对局室里现在能不能下
bool MainWindow::roomAllowsClick()
{
    if (!m_room) return true;
    if (!m_room->canEditNow()) {
        statusBar()->showMessage(m_room->isLecture() ? T("老师在摆棋，你只能看。") : T("现在不轮到你走。"), 3000);
        return false;
    }
    if (!m_room->canManage() && !m_room->isLecture() && roomCursor() != m_room->cursor()) {
        // 指导棋里学生先回到最新局面再走
        statusBar()->showMessage(T("请先回到最新局面再落子。"), 3000);
        applyRoomBoard(Sgf::save(m_tree), m_room->cursor());
        return false;
    }
    return true;
}
