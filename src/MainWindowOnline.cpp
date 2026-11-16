// 在线功能：登录教学服务器、云棋谱、作业、管理。不登录时 NanoGo 就是单机版，这里的东西都不出现。
#include "MainWindow.h"
#include "I18n.h"
#include "Sgf.h"
#include "online/ApiClient.h"
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSizePolicy>
#include <QStatusBar>
#include <QToolBar>

void fillStrengthBox(QComboBox *box, bool verbose); // MainWindow.cpp
QString strengthText(int v);

void MainWindow::buildOnlineMenu()
{
    m_onlineMenu = menuBar()->addMenu(T("在线(&O)"));
    m_loginAct = m_onlineMenu->addAction(T("登录教学服务器…"), this, &MainWindow::onlineLogin);
    m_pwAct = m_onlineMenu->addAction(T("修改密码…"), this, [this] { Online::changePassword(this); });
    m_onlineMenu->addSeparator();
    m_cloudAct = m_onlineMenu->addAction(T("我的云棋谱…"), this, [this] {
        CloudGamesDialog dlg(this, this);
        dlg.exec();
    });
    m_hwAct = m_onlineMenu->addAction(T("作业…"), this, [this] {
        HomeworkDialog dlg(this, this);
        dlg.exec();
    });
    m_adminAct = m_onlineMenu->addAction(T("管理（账号、班级）…"), this, [this] {
        AdminDialog dlg(this);
        dlg.exec();
    });

    m_onlineStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_onlineStatus);
    connect(ApiClient::instance(), &ApiClient::sessionChanged, this, &MainWindow::refreshOnline);
    refreshOnline();
    // 上次记住了登录：后台恢复，连不上也不打扰
    ApiClient::instance()->restore(m_settings.serverUrl, m_settings.serverUser);
}

void MainWindow::addOnlineToolbar(QToolBar *tb)
{
    auto *spacer = new QWidget(tb);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
    m_tbLoginAct = tb->addAction(T("登录"), this, &MainWindow::onlineLogin);
    refreshOnline();
}

void MainWindow::onlineLogin()
{
    ApiClient *api = ApiClient::instance();
    if (api->loggedIn()) {
        if (QMessageBox::question(this, T("退出登录"), T("退出登录？退出后仍可单机使用。")) == QMessageBox::Yes) api->logout();
        return;
    }
    LoginDialog dlg(m_settings.serverUrl, m_settings.serverUser, this);
    // 填过的服务器地址和用户名马上记住：登录失败、点取消或关掉对话框，下次也不用重填
    auto remember = [this](const QString &server, const QString &user) {
        if (server.isEmpty() && user.isEmpty()) return;
        if (server == m_settings.serverUrl && user == m_settings.serverUser) return;
        if (!server.isEmpty()) m_settings.serverUrl = server;
        if (!user.isEmpty()) m_settings.serverUser = user;
        m_settings.save();
    };
    connect(&dlg, &LoginDialog::attempted, this, remember);
    dlg.exec();
    remember(dlg.server(), dlg.user());
}

void MainWindow::refreshOnline()
{
    if (!m_loginAct) return;
    ApiClient *api = ApiClient::instance();
    const bool in = api->loggedIn();
    m_loginAct->setText(in ? T("退出登录") : T("登录教学服务器…"));
    for (QAction *a : {m_pwAct, m_cloudAct, m_hwAct}) a->setVisible(in);
    m_adminAct->setVisible(in && api->isAdmin());
    m_hwAct->setText(api->isTeacher() ? T("布置和批改作业…") : T("我的作业…"));
    const QString name = api->me().value("display_name").toString();
    if (m_tbLoginAct) {
        m_tbLoginAct->setText(in ? name : T("登录"));
        m_tbLoginAct->setToolTip(in ? T("已登录：%1（%2），点击退出登录").arg(name, ApiClient::roleName(api->role()))
                                    : T("登录教学服务器（不登录也可以单机使用）"));
    }
    m_onlineStatus->setText(in ? T("已登录：%1（%2）").arg(name, ApiClient::roleName(api->role())) : QString());
}

// ---- OnlineHost ----

QString MainWindow::currentGameSgf() const
{
    return Sgf::save(m_tree);
}

void MainWindow::loadSgfText(const QString &sgf, const QString &title)
{
    autosave();
    loadSgfContent(sgf, title.isEmpty() ? T("云棋谱") : title, QString());
}

bool MainWindow::startTsumegoSet(const QString &key, const QString &name, const QString &sgf)
{
    return useTsProblems(Tsumego::loadCollection(sgf), key, name);
}

int MainWindow::tsumegoSolved(const QString &key, int total) const
{
    QSettings st(QDir(AppSettings::dataDir()).filePath("tsumego.ini"), QSettings::IniFormat);
    const QString g = tsProgressGroupFor(key);
    int n = 0;
    for (int i = 0; i < total; ++i)
        if (st.value(g + QStringLiteral("/solved%1").arg(i)).toBool()) ++n;
    return n;
}

void MainWindow::startAiGameAt(int level)
{
    // 人机对弈对话框默认选中当前强度
    m_aiVisits = level;
    newAiGame();
}

void MainWindow::fillStrengthChoices(QComboBox *box) const
{
    fillStrengthBox(box, true);
}

QString MainWindow::strengthLabel(int level) const
{
    return strengthText(level);
}
