// 对局室（二期）：指导棋和班级大课。进房后主棋盘和服务器同步，离开后恢复原来的棋局。
#include "MainWindow.h"
#include "BoardWidget.h"
#include "I18n.h"
#include "Sgf.h"
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

void MainWindow::openRooms()
{
    if (!ApiClient::instance()->loggedIn()) return;
    RoomsDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted && dlg.chosenRoom()) enterRoom(dlg.chosenRoom());
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
    if (m_tsMode) exitTsumego();
    autosave();  // 进房前的棋局存好，离开时恢复
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
        QTimer::singleShot(0, this, &MainWindow::leaveRoom);
        QMessageBox::information(this, T("对局室"), byMe ? T("对局室已结束，棋谱已存进你的云棋谱。")
                                 : game ? T("老师结束了对局室，棋谱已存进你的云棋谱。") : T("老师结束了对局室。"));
    });
    m_room->start();
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
    restoreAutosave();
    positionChanged();
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
