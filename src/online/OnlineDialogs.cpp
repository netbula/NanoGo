#include "OnlineDialogs.h"
#include "../AppSettings.h"
#include <QDir>
#include <QSettings>
#include "ApiClient.h"
#include "../I18n.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <memory>
#include "../Sgf.h"
#include "SgfPicker.h"

namespace {
ApiClient *api() { return ApiClient::instance(); }
qint64 idOf(const QJsonObject &o, const char *key = "id") { return qint64(o.value(key).toDouble()); }

QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setAlternatingRowColors(true);
    return t;
}

void setRow(QTableWidget *t, int row, const QStringList &cells)
{
    for (int c = 0; c < cells.size(); ++c) t->setItem(row, c, new QTableWidgetItem(cells[c]));
}

// 棋谱自己的标题（SGF 的 GN，保存棋谱时填的）
QString sgfTitle(const QString &sgf)
{
    GameTree t;
    if (!Sgf::load(sgf, t, nullptr)) return {};
    return t.root()->extraProps.value("GN").value(0).trimmed();
}
// 上传文件的标题：有棋谱标题用它，没有就用文件名
QString fileTitle(const QString &sgf, const QString &path)
{
    const QString t = sgfTitle(sgf);
    return t.isEmpty() ? QFileInfo(path).completeBaseName() : t;
}

QDialogButtonBox *okCancel(QDialog *dlg, const QString &okText = QString())
{
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText(okText.isEmpty() ? T("确定") : okText);
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return bb;
}
}

// ---- 公用 ----

const char *const Online::kServerDownloadUrl = "https://github.com/netbula/NanoGo/releases?q=server-&expanded=true";

void Online::showServerInfo(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(T("NanoGo 教学服务端"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *text = new QTextBrowser(&dlg);
    text->setOpenExternalLinks(true);
    text->setHtml(T("<h3>用 NanoGo 教围棋：免费的教学服务端</h3>"
                    "<p>学校、围棋班、带学生的老师，可以自己架一台 NanoGo 教学服务端。"
                    "服务端程序<b>免费下载</b>，单个文件，Windows、Linux、macOS 都能用，不用装数据库，几分钟就能架好。"
                    "老师和学生用 NanoGo 登录就能上课。</p>"
                    "<p><b>账号和班级</b>：学生、老师、管理员三种角色，管理员开账号、分班、记段位。</p>"
                    "<p><b>云棋谱</b>：棋谱存到服务器上，老师随时查看自己学生的对局。</p>"
                    "<p><b>作业</b>：布置死活题、复盘、和 AI 下棋三种作业。学生做完提交，老师批改、打分、写评语。</p>"
                    "<p><b>对局室</b>：指导棋（全班可观摩）、班级大课（老师摆棋讲解，全班实时看到）、学生比赛（老师当裁判）。"
                    "支持中国、日本、韩国、应氏等规则，可选读秒计时。下完的棋谱自动存好，双方名字、段位、结果都写好。</p>"
                    "<p><b>防作弊</b>：上课和比赛时学生不能用 AI，NanoGo 还会检查学生电脑上有没有运行其他围棋 AI，发现了就提醒老师。</p>"
                    "<p>架设方法：下载后运行 <code>nanogo-server -init-admin admin</code> 建管理员，再运行 <code>nanogo-server</code> 即可。"
                    "给局域网用加 <code>-addr 0.0.0.0:8080</code>，对外服务放在 HTTPS 后面。详见压缩包里的说明。</p>"));
    text->setMinimumSize(560, 420);
    lay->addWidget(text);
    auto *bb = new QDialogButtonBox(&dlg);
    auto *dl = bb->addButton(T("免费下载服务端"), QDialogButtonBox::AcceptRole);
    bb->addButton(T("关闭"), QDialogButtonBox::RejectRole);
    QObject::connect(dl, &QPushButton::clicked, &dlg, [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kServerDownloadUrl))); });
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);
    dlg.exec();
}

void Online::showError(QWidget *parent, const QString &title, const ApiResult &r)
{
    QMessageBox::warning(parent, title, r.message());
}

QString Online::formatTime(qint64 t)
{
    if (t <= 0) return QString();
    return QDateTime::fromSecsSinceEpoch(t).toString("yyyy-MM-dd HH:mm");
}

QString Online::homeworkKindName(const QString &kind)
{
    if (kind == QLatin1String("tsumego")) return T("做死活题");
    if (kind == QLatin1String("review")) return T("复盘");
    return T("和 AI 下棋");
}

QString Online::homeworkKey(qint64 homeworkId)
{
    return QStringLiteral("hw|%1|%2|%3").arg(api()->baseUrl()).arg(api()->myId()).arg(homeworkId);
}

void Online::changePassword(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(T("修改密码"));
    auto *form = new QFormLayout(&dlg);
    auto *oldPw = new QLineEdit(&dlg), *newPw = new QLineEdit(&dlg), *again = new QLineEdit(&dlg);
    for (QLineEdit *e : {oldPw, newPw, again}) e->setEchoMode(QLineEdit::Password);
    form->addRow(T("原密码："), oldPw);
    form->addRow(T("新密码："), newPw);
    form->addRow(T("再输一次："), again);
    form->addRow(okCancel(&dlg));
    if (dlg.exec() != QDialog::Accepted) return;
    if (newPw->text() != again->text()) {
        QMessageBox::warning(parent, T("修改密码"), T("两次输入的新密码不一样。"));
        return;
    }
    QPointer<QWidget> p(parent);
    api()->put("/me/password", QJsonObject{{"old_password", oldPw->text()}, {"new_password", newPw->text()}},
               [p](const ApiResult &r) {
                   if (!p) return;
                   if (r.ok) QMessageBox::information(p, T("修改密码"), T("密码已修改。其他电脑上的登录需要重新登录。"));
                   else if (r.code == "bad_credentials") QMessageBox::warning(p, T("修改密码"), T("原密码不对。"));
                   else Online::showError(p, T("修改密码"), r);
               });
}

QString Online::rankName(int r)
{
    if (r <= -201) return T("职业 %1 段").arg(-r - 200);
    if (r <= -101) return T("业余 %1 段").arg(-r - 100);
    if (r < 0) return T("%1 级").arg(-r);
    return T("未填");
}

void Online::fillRankBox(QComboBox *box, int current)
{
    box->clear();
    box->addItem(T("未填"), 0);
    for (int k = 25; k >= 1; --k) box->addItem(rankName(-k), -k);
    for (int d = 1; d <= 8; ++d) box->addItem(rankName(-100 - d), -100 - d);
    for (int d = 1; d <= 9; ++d) box->addItem(rankName(-200 - d), -200 - d);
    box->setCurrentIndex(qMax(0, box->findData(current)));
}

void Online::editClassRanks(QWidget *parent, qint64 classId, const QString &className)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(T("学生棋力：%1").arg(className));
    auto *lay = new QVBoxLayout(&dlg);
    QTableWidget *table = makeTable({T("名字"), T("用户名"), T("棋力")}, &dlg);
    lay->addWidget(table, 1);
    auto *btns = new QHBoxLayout;
    auto *setBtn = new QPushButton(T("设置棋力…"), &dlg);
    auto *closeBtn = new QPushButton(T("关闭"), &dlg);
    btns->addWidget(setBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    lay->addLayout(btns);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    auto members = std::make_shared<QJsonArray>();
    QPointer<QDialog> self(&dlg);
    auto reload = [self, table, members, classId] {
        api()->get(QStringLiteral("/classes/%1/members").arg(classId), [self, table, members](const ApiResult &r) {
            if (!self) return;
            if (!r.ok) { Online::showError(self, T("学生棋力"), r); return; }
            *members = r.data.toArray();
            table->setRowCount(members->size());
            for (int i = 0; i < members->size(); ++i) {
                const QJsonObject u = (*members)[i].toObject();
                setRow(table, i, {u["display_name"].toString(), u["username"].toString(), Online::rankName(u["rank"].toInt())});
            }
        });
    };
    auto setRank = [self, table, members, reload] {
        const int row = table->currentRow();
        if (!self || row < 0 || row >= members->size()) return;
        const QJsonObject u = (*members)[row].toObject();
        QDialog d(self);
        d.setWindowTitle(T("设置棋力"));
        auto *form = new QFormLayout(&d);
        auto *box = new QComboBox(&d);
        Online::fillRankBox(box, u["rank"].toInt());
        form->addRow(T("%1 的棋力：").arg(u["display_name"].toString()), box);
        form->addRow(okCancel(&d));
        if (d.exec() != QDialog::Accepted) return;
        api()->put(QStringLiteral("/users/%1/rank").arg(idOf(u)), QJsonObject{{"rank", box->currentData().toInt()}},
                   [self, reload](const ApiResult &r) {
                       if (!self) return;
                       if (!r.ok) Online::showError(self, T("设置棋力"), r);
                       reload();
                   });
    };
    QObject::connect(setBtn, &QPushButton::clicked, &dlg, setRank);
    QObject::connect(table, &QTableWidget::cellDoubleClicked, &dlg, setRank);
    dlg.resize(460, 420);
    reload();
    dlg.exec();
}

// ---- 登录 ----

LoginDialog::LoginDialog(const QString &server, const QString &user, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(T("登录教学服务器"));
    auto *form = new QFormLayout(this);
    m_server = new QLineEdit(server, this);
    m_server->setPlaceholderText(T("例如 go.example.com 或 http://127.0.0.1:8080"));
    m_user = new QLineEdit(user, this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_remember = new QCheckBox(T("记住登录（30 天内不用再输密码）"), this);
    m_remember->setChecked(true);
    m_error = new QLabel(this);
    m_error->setStyleSheet("color: #c33;");
    m_error->setWordWrap(true);
    form->addRow(T("服务器："), m_server);
    form->addRow(T("用户名："), m_user);
    form->addRow(T("密码："), m_password);
    form->addRow(m_remember);
    auto *note = new QLabel(T("账号由老师或管理员开通。不登录也可以照常使用复盘、对弈和死活题。"), this);
    note->setStyleSheet("color: gray;");
    note->setWordWrap(true);
    form->addRow(note);
    auto *promo = new QLabel(QStringLiteral("<a href='#'>%1</a>").arg(T("还没有服务器？免费架设 NanoGo 教学服务端")), this);
    connect(promo, &QLabel::linkActivated, this, [this] { Online::showServerInfo(this); });
    form->addRow(promo);
    form->addRow(m_error);
    auto *bb = okCancel(this, T("登录"));
    m_ok = bb->button(QDialogButtonBox::Ok);
    disconnect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::accepted, this, &LoginDialog::tryLogin);
    form->addRow(bb);
    (server.isEmpty() ? m_server : user.isEmpty() ? m_user : m_password)->setFocus();
    resize(420, sizeHint().height());
}

QString LoginDialog::server() const { return ApiClient::normalizeUrl(m_server->text()); }
QString LoginDialog::user() const { return m_user->text().trimmed().toLower(); }

void LoginDialog::tryLogin()
{
    if (m_server->text().trimmed().isEmpty() || m_user->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_error->setText(T("请填写服务器、用户名和密码。"));
        return;
    }
    emit attempted(server(), user());
    m_ok->setEnabled(false);
    m_error->setText(T("正在登录…"));
    QPointer<LoginDialog> self(this);
    api()->login(m_server->text(), m_user->text(), m_password->text(), m_remember->isChecked(), [self](const ApiResult &r) {
        if (!self) return;
        self->m_ok->setEnabled(true);
        if (r.ok) self->accept();
        else self->m_error->setText(r.message());
    });
}

// ---- 云棋谱 ----

CloudGamesDialog::CloudGamesDialog(OnlineHost *host, QWidget *parent) : QDialog(parent), m_host(host)
{
    setWindowTitle(T("我的云棋谱"));
    auto *lay = new QVBoxLayout(this);
    if (api()->isTeacher()) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(T("查看："), this));
        m_owner = new QComboBox(this);
        m_owner->addItem(T("我自己"), api()->myId());
        row->addWidget(m_owner, 1);
        lay->addLayout(row);
        QPointer<CloudGamesDialog> self(this);
        // 管理员能看所有老师和学生的棋谱；老师看自己班的学生
        api()->get("/users", [self](const ApiResult &r) {
            if (!self || !r.ok) return;
            for (const QJsonValue &v : r.data.toArray()) {
                const QJsonObject u = v.toObject();
                if (idOf(u) == api()->myId()) continue;
                const QString who = u["role"].toString() == "student" ? T("学生：%1（%2）") : T("%1：%2（%3）").arg(ApiClient::roleName(u["role"].toString()), "%1", "%2");
                self->m_owner->addItem(who.arg(u["display_name"].toString(), u["username"].toString()), idOf(u));
            }
        });
        connect(m_owner, &QComboBox::currentIndexChanged, this, &CloudGamesDialog::reload);
    }
    m_table = makeTable({T("标题"), T("黑方"), T("白方"), T("结果"), T("上传时间")}, this);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &CloudGamesDialog::openSelected);
    lay->addWidget(m_table, 1);
    auto *btns = new QHBoxLayout;
    auto *openBtn = new QPushButton(T("打开"), this);
    m_uploadBtn = new QPushButton(T("上传当前棋局…"), this);
    m_uploadFilesBtn = new QPushButton(T("从文件上传…"), this);
    m_uploadFilesBtn->setToolTip(T("选一个或多个 SGF 棋谱文件上传到云棋谱"));
    m_renameBtn = new QPushButton(T("改标题…"), this);
    m_deleteBtn = new QPushButton(T("删除"), this);
    auto *closeBtn = new QPushButton(T("关闭"), this);
    btns->addWidget(openBtn);
    btns->addWidget(m_uploadBtn);
    btns->addWidget(m_uploadFilesBtn);
    btns->addWidget(m_renameBtn);
    btns->addWidget(m_deleteBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    lay->addLayout(btns);
    connect(openBtn, &QPushButton::clicked, this, &CloudGamesDialog::openSelected);
    connect(m_uploadBtn, &QPushButton::clicked, this, &CloudGamesDialog::upload);
    connect(m_uploadFilesBtn, &QPushButton::clicked, this, &CloudGamesDialog::uploadFiles);
    connect(m_renameBtn, &QPushButton::clicked, this, &CloudGamesDialog::renameSelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CloudGamesDialog::deleteSelected);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    resize(760, 460);
    reload();
}

qint64 CloudGamesDialog::selectedId() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_games.size() ? idOf(m_games[row].toObject()) : 0;
}

void CloudGamesDialog::reload()
{
    const qint64 owner = m_owner ? m_owner->currentData().toLongLong() : api()->myId();
    const bool mine = owner == api()->myId();
    m_uploadBtn->setEnabled(mine);
    m_uploadFilesBtn->setEnabled(mine);
    m_deleteBtn->setEnabled(mine || api()->isAdmin());
    m_renameBtn->setEnabled(mine || api()->isAdmin());
    QPointer<CloudGamesDialog> self(this);
    api()->get(QStringLiteral("/games?owner=%1").arg(owner), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("我的云棋谱"), r); return; }
        self->m_games = r.data.toArray();
        self->m_table->setRowCount(self->m_games.size());
        for (int i = 0; i < self->m_games.size(); ++i) {
            const QJsonObject g = self->m_games[i].toObject();
            setRow(self->m_table, i, {g["title"].toString(), g["black"].toString(), g["white"].toString(),
                                      g["result"].toString(), Online::formatTime(idOf(g, "created_at"))});
        }
    });
}

void CloudGamesDialog::openSelected()
{
    const qint64 id = selectedId();
    if (!id) return;
    QPointer<CloudGamesDialog> self(this);
    api()->get(QStringLiteral("/games/%1").arg(id), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("打开"), r); return; }
        const QJsonObject g = r.data.toObject();
        self->m_host->loadSgfText(g["sgf"].toString(), g["title"].toString());
        self->accept();
    });
}

void CloudGamesDialog::upload()
{
    bool ok = false;
    // 默认用保存棋谱时填的标题
    const QString title = QInputDialog::getText(this, T("上传当前棋局"), T("标题（可以不填）："), QLineEdit::Normal,
                                                sgfTitle(m_host->currentGameSgf()), &ok);
    if (!ok) return;
    QPointer<CloudGamesDialog> self(this);
    api()->post("/games", QJsonObject{{"sgf", m_host->currentGameSgf()}, {"title", title}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("上传当前棋局"), r); return; }
        self->reload();
    });
}

void CloudGamesDialog::uploadFiles()
{
    // 记住上次选文件的文件夹，下次直接打开那里
    QSettings st(QDir(AppSettings::dataDir()).filePath("picker.ini"), QSettings::IniFormat);
    // 确定按钮写成“上传”，不叫“打开”
    QFileDialog fd(this, T("选择要上传的棋谱（可以多选）"), st.value("uploadDir").toString(), T("SGF 棋谱 (*.sgf);;所有文件 (*)"));
    fd.setFileMode(QFileDialog::ExistingFiles);
    fd.setLabelText(QFileDialog::Accept, T("上传"));
    fd.setLabelText(QFileDialog::Reject, T("取消"));
    if (fd.exec() != QDialog::Accepted) return;
    const QStringList paths = fd.selectedFiles();
    if (paths.isEmpty()) return;
    st.setValue("uploadDir", QFileInfo(paths.first()).absolutePath());
    m_uploadFilesBtn->setEnabled(false);
    // 一个一个传，全部传完再刷新；出错的记下来最后一起说
    auto failed = std::make_shared<QStringList>();
    auto left = std::make_shared<int>(int(paths.size()));
    QPointer<CloudGamesDialog> self(this);
    for (const QString &path : paths) {
        QFile f(path);
        const QString name = QFileInfo(path).fileName();
        if (!f.open(QIODevice::ReadOnly)) {
            failed->append(name);
            if (--*left == 0) { m_uploadFilesBtn->setEnabled(true); reload(); }
            continue;
        }
        const QString sgf = Sgf::decode(f.readAll());
        api()->post("/games", QJsonObject{{"sgf", sgf}, {"title", fileTitle(sgf, path)}}, [self, name, failed, left](const ApiResult &r) {
            if (!r.ok) failed->append(QStringLiteral("%1（%2）").arg(name, r.message()));
            if (--*left > 0 || !self) return;
            self->m_uploadFilesBtn->setEnabled(true);
            self->reload();
            if (!failed->isEmpty())
                QMessageBox::warning(self, T("从文件上传…"), T("这些文件没有传上去：\n%1").arg(failed->join('\n')));
        });
    }
}

void CloudGamesDialog::renameSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_games.size()) return;
    const QJsonObject g = m_games[row].toObject();
    bool ok = false;
    const QString title = QInputDialog::getText(this, T("改标题"), T("新标题："), QLineEdit::Normal, g["title"].toString(), &ok);
    if (!ok) return;
    QPointer<CloudGamesDialog> self(this);
    api()->patch(QStringLiteral("/games/%1").arg(idOf(g)), QJsonObject{{"title", title.trimmed()}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("改标题"), r);
        self->reload();
    });
}

void CloudGamesDialog::deleteSelected()
{
    const qint64 id = selectedId();
    if (!id) return;
    if (QMessageBox::question(this, T("删除"), T("从服务器上删除这盘棋谱？删除后不能恢复。")) != QMessageBox::Yes) return;
    QPointer<CloudGamesDialog> self(this);
    api()->del(QStringLiteral("/games/%1").arg(id), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("删除"), r);
        self->reload();
    });
}

// ---- 作业 ----

HomeworkDialog::HomeworkDialog(OnlineHost *host, QWidget *parent)
    : QDialog(parent), m_host(host), m_teacher(api()->isTeacher())
{
    setWindowTitle(m_teacher ? T("布置和批改作业") : T("我的作业"));
    auto *lay = new QVBoxLayout(this);
    if (m_teacher) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(T("班级："), this));
        m_class = new QComboBox(this);
        row->addWidget(m_class, 1);
        lay->addLayout(row);
        connect(m_class, &QComboBox::currentIndexChanged, this, &HomeworkDialog::reload);
        m_table = makeTable({T("作业"), T("类型"), T("截止"), T("已交"), T("已批改")}, this);
    } else {
        m_table = makeTable({T("作业"), T("类型"), T("班级"), T("截止"), T("状态")}, this);
    }
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    auto *split = new QSplitter(Qt::Vertical, this);
    split->addWidget(m_table);
    m_detail = new QTextBrowser(this);
    split->addWidget(m_detail);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);
    connect(m_table, &QTableWidget::currentCellChanged, this, &HomeworkDialog::showDetail);

    auto *btns = new QHBoxLayout;
    if (m_teacher) {
        m_assignBtn = new QPushButton(T("布置作业…"), this);
        m_subsBtn = new QPushButton(T("查看提交…"), this);
        m_deleteBtn = new QPushButton(T("删除"), this);
        m_ranksBtn = new QPushButton(T("学生棋力…"), this);
        btns->addWidget(m_assignBtn);
        btns->addWidget(m_subsBtn);
        btns->addWidget(m_deleteBtn);
        btns->addWidget(m_ranksBtn);
        connect(m_ranksBtn, &QPushButton::clicked, this, [this] {
            if (m_class->count() == 0) return;
            Online::editClassRanks(this, m_class->currentData().toLongLong(), m_class->currentText());
        });
        connect(m_assignBtn, &QPushButton::clicked, this, &HomeworkDialog::assign);
        connect(m_subsBtn, &QPushButton::clicked, this, &HomeworkDialog::viewSubmissions);
        connect(m_table, &QTableWidget::cellDoubleClicked, this, &HomeworkDialog::viewSubmissions);
        connect(m_deleteBtn, &QPushButton::clicked, this, &HomeworkDialog::deleteSelected);
    } else {
        m_startBtn = new QPushButton(T("开始做"), this);
        m_submitBtn = new QPushButton(T("提交…"), this);
        btns->addWidget(m_startBtn);
        btns->addWidget(m_submitBtn);
        connect(m_startBtn, &QPushButton::clicked, this, &HomeworkDialog::startSelected);
        connect(m_table, &QTableWidget::cellDoubleClicked, this, &HomeworkDialog::startSelected);
        connect(m_submitBtn, &QPushButton::clicked, this, &HomeworkDialog::submitSelected);
    }
    btns->addStretch();
    auto *closeBtn = new QPushButton(T("关闭"), this);
    btns->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    lay->addLayout(btns);
    resize(780, 520);
    if (m_teacher) reloadClasses();
    else reload();
}

void HomeworkDialog::reloadClasses()
{
    QPointer<HomeworkDialog> self(this);
    api()->get("/classes", [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("作业"), r); return; }
        self->m_class->blockSignals(true);
        self->m_class->clear();
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject c = v.toObject();
            self->m_class->addItem(T("%1（%2 人）").arg(c["name"].toString()).arg(c["students"].toInt()), idOf(c));
        }
        self->m_class->blockSignals(false);
        if (self->m_class->count() == 0)
            self->m_detail->setText(T("你还没有带班。请管理员在“管理”里建班并把你设为任课老师。"));
        self->m_assignBtn->setEnabled(self->m_class->count() > 0);
        self->m_ranksBtn->setEnabled(self->m_class->count() > 0);
        self->reload();
    });
}

QJsonObject HomeworkDialog::selected() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_list.size() ? m_list[row].toObject() : QJsonObject();
}

void HomeworkDialog::reload()
{
    QString path = "/homework";
    if (m_teacher) {
        if (m_class->count() == 0) { m_list = {}; m_table->setRowCount(0); return; }
        path += QStringLiteral("?class_id=%1").arg(m_class->currentData().toLongLong());
    }
    const qint64 keep = idOf(selected());
    QPointer<HomeworkDialog> self(this);
    api()->get(path, [self, keep](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("作业"), r); return; }
        self->m_list = r.data.toArray();
        QTableWidget *t = self->m_table;
        t->setRowCount(self->m_list.size());
        int select = self->m_list.isEmpty() ? -1 : 0;
        for (int i = 0; i < self->m_list.size(); ++i) {
            const QJsonObject h = self->m_list[i].toObject();
            if (idOf(h) == keep) select = i;
            const QString due = h["due_at"].isNull() ? T("不限") : Online::formatTime(idOf(h, "due_at"));
            if (self->m_teacher) {
                setRow(t, i, {h["title"].toString(), Online::homeworkKindName(h["kind"].toString()), due,
                              QStringLiteral("%1 / %2").arg(h["submitted"].toInt()).arg(h["targets"].toInt()),
                              QString::number(h["reviewed"].toInt())});
            } else {
                const QJsonObject mine = h["my_submission"].toObject();
                QString status = T("未交");
                if (!mine.isEmpty()) {
                    status = T("已交");
                    if (!mine["reviewed_at"].isNull())
                        status = mine["score"].isNull() ? T("已批改") : T("已批改：%1 分").arg(mine["score"].toInt());
                }
                setRow(t, i, {h["title"].toString(), Online::homeworkKindName(h["kind"].toString()),
                              h["class_name"].toString(), due, status});
            }
        }
        if (select >= 0) t->selectRow(select);
        self->showDetail();
    });
}

void HomeworkDialog::showDetail()
{
    const QJsonObject h = selected();
    const bool any = !h.isEmpty();
    if (m_startBtn) { m_startBtn->setEnabled(any); m_submitBtn->setEnabled(any); }
    if (m_subsBtn) { m_subsBtn->setEnabled(any); m_deleteBtn->setEnabled(any); }
    if (!any) { if (!m_teacher) m_detail->setText(m_list.isEmpty() ? T("现在没有作业。") : QString()); return; }
    QString html = "<h3>" + h["title"].toString().toHtmlEscaped() + "</h3>";
    const QString kind = h["kind"].toString();
    QString what;
    if (kind == "tsumego") what = T("做完老师布置的死活题集，做完后点“提交”。");
    else if (kind == "review") what = T("打开老师指定的棋谱复盘，在棋谱里写下你的评注，然后点“提交”交上当前棋局。");
    else what = T("和 AI（%1）下 %2 盘，每下完一盘点“提交”交上当前棋局。")
                   .arg(m_host->strengthLabel(h["ai_level"].toInt())).arg(h["ai_games"].toInt());
    html += "<p>" + what.toHtmlEscaped() + "</p>";
    if (!h["description"].toString().isEmpty())
        html += "<p><b>" + T("老师的说明：").toHtmlEscaped() + "</b>" + h["description"].toString().toHtmlEscaped().replace("\n", "<br>") + "</p>";
    html += "<p style='color:gray'>" + T("老师：%1　布置时间：%2").arg(h["teacher_name"].toString(), Online::formatTime(idOf(h, "created_at"))).toHtmlEscaped() + "</p>";
    const QJsonObject mine = h["my_submission"].toObject();
    if (!mine.isEmpty()) {
        QString s = T("已于 %1 提交。").arg(Online::formatTime(idOf(mine, "submitted_at")));
        if (kind == "tsumego") s += T("做对 %1 / %2 题。").arg(mine["solved"].toInt()).arg(mine["total"].toInt());
        else s += T("交了 %1 盘棋。").arg(mine["game_ids"].toArray().size());
        html += "<p>" + s.toHtmlEscaped() + "</p>";
        if (!mine["reviewed_at"].isNull()) {
            QString rv = mine["score"].isNull() ? T("老师已批改。") : T("老师已批改：%1 分。").arg(mine["score"].toInt());
            html += "<p><b>" + rv.toHtmlEscaped() + "</b> " + mine["comment"].toString().toHtmlEscaped().replace("\n", "<br>") + "</p>";
        }
    }
    m_detail->setHtml(html);
}

void HomeworkDialog::startSelected()
{
    const QJsonObject h = selected();
    if (h.isEmpty()) return;
    const QString kind = h["kind"].toString();
    QPointer<HomeworkDialog> self(this);
    if (kind == "tsumego") {
        api()->get(QStringLiteral("/problemsets/%1").arg(idOf(h, "problem_set_id")), [self, h](const ApiResult &r) {
            if (!self) return;
            if (!r.ok) { Online::showError(self, T("开始做"), r); return; }
            if (self->m_host->startTsumegoSet(Online::homeworkKey(idOf(h)), h["title"].toString(), r.data.toObject()["sgf"].toString()))
                self->accept();
        });
    } else if (kind == "review") {
        if (h["game_id"].isNull()) { QMessageBox::warning(this, T("开始做"), T("老师指定的棋谱已被删除。")); return; }
        api()->get(QStringLiteral("/games/%1").arg(idOf(h, "game_id")), [self](const ApiResult &r) {
            if (!self) return;
            if (!r.ok) { Online::showError(self, T("开始做"), r); return; }
            const QJsonObject g = r.data.toObject();
            self->m_host->loadSgfText(g["sgf"].toString(), g["title"].toString());
            self->accept();
        });
    } else {
        accept();
        m_host->startAiGameAt(h["ai_level"].toInt());
    }
}

void HomeworkDialog::submitSelected()
{
    const QJsonObject h = selected();
    if (h.isEmpty()) return;
    const QString kind = h["kind"].toString();
    const QJsonObject mine = h["my_submission"].toObject();
    if (kind == "tsumego") {
        QPointer<HomeworkDialog> self(this);
        api()->get(QStringLiteral("/problemsets/%1").arg(idOf(h, "problem_set_id")), [self, h](const ApiResult &r) {
            if (!self) return;
            if (!r.ok) { Online::showError(self, T("提交"), r); return; }
            const int total = r.data.toObject()["problem_count"].toInt();
            const int solved = self->m_host->tsumegoSolved(Online::homeworkKey(idOf(h)), total);
            bool ok = false;
            const QString note = QInputDialog::getMultiLineText(self, T("提交作业"),
                T("你在这台电脑上做对了 %1 / %2 题。\n想对老师说的话（可以不填）：").arg(solved).arg(total), QString(), &ok);
            if (!ok || !self) return;
            api()->put(QStringLiteral("/homework/%1/submission").arg(idOf(h)),
                       QJsonObject{{"solved", solved}, {"total", total}, {"note", note}}, [self](const ApiResult &r2) {
                           if (!self) return;
                           if (!r2.ok) { Online::showError(self, T("提交"), r2); return; }
                           QMessageBox::information(self, T("提交作业"), T("已提交。"));
                           self->reload();
                       });
        });
        return;
    }
    // 复盘和对 AI：把当前棋盘上的棋局上传后交上去
    QString prompt = kind == "review" ? T("会把当前棋盘上的棋谱（连同你写的评注）交上去。")
                                      : T("会把当前棋盘上的这盘棋交上去（已交 %1 盘，要求 %2 盘）。")
                                            .arg(mine["game_ids"].toArray().size()).arg(h["ai_games"].toInt());
    bool ok = false;
    const QString note = QInputDialog::getMultiLineText(this, T("提交作业"), prompt + "\n" + T("想对老师说的话（可以不填）："),
                                                        mine["note"].toString(), &ok);
    if (!ok) return;
    QPointer<HomeworkDialog> self(this);
    api()->post("/games", QJsonObject{{"sgf", m_host->currentGameSgf()}, {"title", h["title"].toString()}, {"source", "homework"}},
                [self, h, mine, kind, note](const ApiResult &r) {
                    if (!self) return;
                    if (!r.ok) { Online::showError(self, T("提交"), r); return; }
                    QJsonArray ids;
                    if (kind == "ai_games") ids = mine["game_ids"].toArray();
                    ids.append(r.data.toObject()["id"]);
                    self->submitWithGames(h, ids, note);
                });
}

void HomeworkDialog::submitWithGames(const QJsonObject &hw, const QJsonArray &gameIds, const QString &note)
{
    QPointer<HomeworkDialog> self(this);
    api()->put(QStringLiteral("/homework/%1/submission").arg(idOf(hw)), QJsonObject{{"game_ids", gameIds}, {"note", note}},
               [self](const ApiResult &r) {
                   if (!self) return;
                   if (!r.ok) { Online::showError(self, T("提交"), r); return; }
                   QMessageBox::information(self, T("提交作业"), T("已提交。"));
                   self->reload();
               });
}

void HomeworkDialog::assign()
{
    if (!m_class || m_class->count() == 0) return;
    AssignHomeworkDialog dlg(m_host, m_class->currentData().toLongLong(), this);
    if (dlg.exec() == QDialog::Accepted) reloadClasses();
}

void HomeworkDialog::viewSubmissions()
{
    const QJsonObject h = selected();
    if (h.isEmpty()) return;
    SubmissionsDialog dlg(m_host, h, this);
    dlg.exec();
    reload();
}

void HomeworkDialog::deleteSelected()
{
    const QJsonObject h = selected();
    if (h.isEmpty()) return;
    if (QMessageBox::question(this, T("删除"), T("删除作业“%1”？学生交的内容也会一起删除。").arg(h["title"].toString())) != QMessageBox::Yes)
        return;
    QPointer<HomeworkDialog> self(this);
    api()->del(QStringLiteral("/homework/%1").arg(idOf(h)), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("删除"), r);
        self->reload();
    });
}

// ---- 布置作业 ----

AssignHomeworkDialog::AssignHomeworkDialog(OnlineHost *host, qint64 classId, QWidget *parent)
    : QDialog(parent), m_host(host), m_classId(classId)
{
    setWindowTitle(T("布置作业"));
    auto *form = new QFormLayout(this);
    m_kind = new QComboBox(this);
    m_kind->addItem(T("做死活题"), "tsumego");
    m_kind->addItem(T("复盘"), "review");
    m_kind->addItem(T("和 AI 下棋"), "ai_games");
    form->addRow(T("类型："), m_kind);
    m_title = new QLineEdit(this);
    form->addRow(T("标题："), m_title);

    m_tsRow = new QWidget(this);
    auto *tsLay = new QHBoxLayout(m_tsRow);
    tsLay->setContentsMargins(0, 0, 0, 0);
    m_problemSet = new QComboBox(m_tsRow);
    auto *browsePs = new QPushButton(T("浏览选择…"), m_tsRow);
    browsePs->setToolTip(T("从服务器题集、自带题、本地文件或文件夹里挑题，可以只选其中几道"));
    tsLay->addWidget(m_problemSet, 1);
    tsLay->addWidget(browsePs);
    form->addRow(T("题集："), m_tsRow);
    connect(browsePs, &QPushButton::clicked, this, &AssignHomeworkDialog::browseProblems);

    m_reviewRow = new QWidget(this);
    auto *rvLay = new QHBoxLayout(m_reviewRow);
    rvLay->setContentsMargins(0, 0, 0, 0);
    m_gameLabel = new QLabel(T("当前棋盘上的棋谱"), m_reviewRow);
    auto *browseGame = new QPushButton(T("浏览选择…"), m_reviewRow);
    browseGame->setToolTip(T("从当前棋盘、云棋谱、本地文件或文件夹里挑一盘棋"));
    rvLay->addWidget(m_gameLabel, 1);
    rvLay->addWidget(browseGame);
    form->addRow(T("棋谱："), m_reviewRow);
    connect(browseGame, &QPushButton::clicked, this, &AssignHomeworkDialog::browseGame);

    m_aiRow = new QWidget(this);
    auto *aiLay = new QHBoxLayout(m_aiRow);
    aiLay->setContentsMargins(0, 0, 0, 0);
    m_aiGames = new QSpinBox(m_aiRow);
    m_aiGames->setRange(1, 100);
    m_aiGames->setValue(3);
    m_aiGames->setSuffix(T(" 盘"));
    m_strength = new QComboBox(m_aiRow);
    m_host->fillStrengthChoices(m_strength);
    const int k5 = m_strength->findData(-5);
    if (k5 >= 0) m_strength->setCurrentIndex(k5);
    aiLay->addWidget(m_aiGames);
    aiLay->addWidget(m_strength, 1);
    form->addRow(T("对局："), m_aiRow);

    m_desc = new QPlainTextEdit(this);
    m_desc->setPlaceholderText(T("给学生的说明（可以不填）"));
    m_desc->setMaximumHeight(80);
    form->addRow(T("说明："), m_desc);
    auto *dueRow = new QHBoxLayout;
    m_hasDue = new QCheckBox(T("截止时间"), this);
    m_hasDue->setChecked(true);
    m_due = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), this);
    m_due->setDisplayFormat("yyyy-MM-dd HH:mm");
    m_due->setCalendarPopup(true);
    dueRow->addWidget(m_hasDue);
    dueRow->addWidget(m_due, 1);
    form->addRow(dueRow);
    connect(m_hasDue, &QCheckBox::toggled, m_due, &QWidget::setEnabled);
    m_students = new QListWidget(this);
    m_students->setMaximumHeight(140);
    form->addRow(T("布置给："), m_students);

    auto *bb = okCancel(this, T("布置"));
    m_ok = bb->button(QDialogButtonBox::Ok);
    disconnect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::accepted, this, &AssignHomeworkDialog::submit);
    form->addRow(bb);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &AssignHomeworkDialog::kindChanged);
    kindChanged();
    loadProblemSets();

    QPointer<AssignHomeworkDialog> self(this);
    api()->get(QStringLiteral("/classes/%1/members").arg(classId), [self](const ApiResult &r) {
        if (!self || !r.ok) return;
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject u = v.toObject();
            auto *it = new QListWidgetItem(u["display_name"].toString(), self->m_students);
            it->setData(Qt::UserRole, idOf(u));
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Checked);
        }
    });
    resize(520, sizeHint().height());
}

void AssignHomeworkDialog::kindChanged()
{
    const QString k = m_kind->currentData().toString();
    auto *form = static_cast<QFormLayout *>(layout());
    form->setRowVisible(m_tsRow, k == "tsumego");
    form->setRowVisible(m_reviewRow, k == "review");
    form->setRowVisible(m_aiRow, k == "ai_games");
}

void AssignHomeworkDialog::loadProblemSets(qint64 select)
{
    QPointer<AssignHomeworkDialog> self(this);
    api()->get("/problemsets", [self, select](const ApiResult &r) {
        if (!self || !r.ok) return;
        self->m_problemSet->clear();
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject p = v.toObject();
            self->m_problemSet->addItem(T("%1（%2 题）").arg(p["title"].toString()).arg(p["problem_count"].toInt()), idOf(p));
        }
        const int i = self->m_problemSet->findData(select);
        if (i >= 0) self->m_problemSet->setCurrentIndex(i);
    });
}

void AssignHomeworkDialog::browseProblems()
{
    ProblemPickerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QList<TsumegoProblem> list = dlg.problems();
    if (list.isEmpty()) return;
    QString name = dlg.collectionName();
    if (m_title->text().trimmed().isEmpty()) m_title->setText(name);
    if (const qint64 id = dlg.serverSetId()) { loadProblemSets(id); return; }
    // 自带题、本地题或只挑了几道：存成服务器上的一个新题集
    if (!dlg.allChecked()) name = T("%1（选 %2 题）").arg(name).arg(list.size());
    m_ok->setEnabled(false);
    QPointer<AssignHomeworkDialog> self(this);
    api()->post("/problemsets", QJsonObject{{"title", name}, {"sgf", Tsumego::saveCollection(list)}}, [self](const ApiResult &r) {
        if (!self) return;
        self->m_ok->setEnabled(true);
        if (!r.ok) { Online::showError(self, T("选择死活题"), r); return; }
        self->loadProblemSets(idOf(r.data.toObject()));
    });
}

void AssignHomeworkDialog::browseGame()
{
    GamePickerDialog dlg(m_host, this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_gameId = dlg.gameId();
    m_gameSgf = m_gameId ? QString() : dlg.sgf();
    const QString t = dlg.title();
    m_gameLabel->setText(t.isEmpty() ? T("当前棋盘上的棋谱") : t);
    if (m_title->text().trimmed().isEmpty() && !t.isEmpty()) m_title->setText(T("复盘：%1").arg(t));
}

void AssignHomeworkDialog::submit()
{
    const QString kind = m_kind->currentData().toString();
    QString title = m_title->text().trimmed();
    if (title.isEmpty()) title = m_kind->currentText();
    QJsonArray students;
    int total = 0;
    for (int i = 0; i < m_students->count(); ++i) {
        ++total;
        if (m_students->item(i)->checkState() == Qt::Checked) students.append(m_students->item(i)->data(Qt::UserRole).toLongLong());
    }
    if (students.isEmpty()) { QMessageBox::warning(this, T("布置作业"), T("请至少选一个学生。")); return; }
    QJsonObject body{{"class_id", m_classId}, {"kind", kind}, {"title", title}, {"description", m_desc->toPlainText()}};
    if (students.size() < total) body["student_ids"] = students;
    if (m_hasDue->isChecked()) body["due_at"] = m_due->dateTime().toSecsSinceEpoch();
    if (kind == "tsumego") {
        if (m_problemSet->currentIndex() < 0) { QMessageBox::warning(this, T("布置作业"), T("请先选择或上传一个题集。")); return; }
        body["problem_set_id"] = m_problemSet->currentData().toLongLong();
    } else if (kind == "ai_games") {
        body["ai_games"] = m_aiGames->value();
        body["ai_level"] = m_strength->currentData().toInt();
    }
    m_ok->setEnabled(false);
    QPointer<AssignHomeworkDialog> self(this);
    auto create = [self](QJsonObject b) {
        api()->post("/homework", b, [self](const ApiResult &r) {
            if (!self) return;
            self->m_ok->setEnabled(true);
            if (!r.ok) { Online::showError(self, T("布置作业"), r); return; }
            self->accept();
        });
    };
    if (kind != "review") { create(body); return; }
    if (m_gameId) { body["game_id"] = m_gameId; create(body); return; }
    const QString sgf = m_gameSgf.isEmpty() ? m_host->currentGameSgf() : m_gameSgf;
    api()->post("/games", QJsonObject{{"sgf", sgf}, {"title", title}}, [self, body, create](const ApiResult &r) mutable {
        if (!self) return;
        if (!r.ok) { self->m_ok->setEnabled(true); Online::showError(self, T("布置作业"), r); return; }
        body["game_id"] = r.data.toObject()["id"];
        create(body);
    });
}

// ---- 查看提交、批改 ----

SubmissionsDialog::SubmissionsDialog(OnlineHost *host, const QJsonObject &homework, QWidget *parent)
    : QDialog(parent), m_host(host), m_hw(homework)
{
    setWindowTitle(T("查看提交：%1").arg(homework["title"].toString()));
    auto *lay = new QVBoxLayout(this);
    m_table = makeTable({T("学生"), T("状态"), T("提交时间"), T("成绩"), T("分数"), T("学生留言"), T("评语")}, this);
    lay->addWidget(m_table, 1);
    auto *btns = new QHBoxLayout;
    auto *openBtn = new QPushButton(T("打开棋谱"), this);
    auto *reviewBtn = new QPushButton(T("批改…"), this);
    auto *closeBtn = new QPushButton(T("关闭"), this);
    openBtn->setVisible(homework["kind"].toString() != "tsumego");
    btns->addWidget(openBtn);
    btns->addWidget(reviewBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    lay->addLayout(btns);
    connect(openBtn, &QPushButton::clicked, this, &SubmissionsDialog::openGame);
    connect(reviewBtn, &QPushButton::clicked, this, &SubmissionsDialog::review);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &SubmissionsDialog::review);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    resize(820, 420);
    reload();
}

QJsonObject SubmissionsDialog::selectedSubmission() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_rows.size() ? m_rows[row].toObject()["submission"].toObject() : QJsonObject();
}

void SubmissionsDialog::reload()
{
    QPointer<SubmissionsDialog> self(this);
    api()->get(QStringLiteral("/homework/%1/submissions").arg(idOf(m_hw)), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("查看提交"), r); return; }
        self->m_rows = r.data.toArray();
        self->m_table->setRowCount(self->m_rows.size());
        const bool ts = self->m_hw["kind"].toString() == "tsumego";
        for (int i = 0; i < self->m_rows.size(); ++i) {
            const QJsonObject row = self->m_rows[i].toObject();
            const QJsonObject s = row["submission"].toObject();
            if (s.isEmpty()) {
                setRow(self->m_table, i, {row["student_name"].toString(), T("未交"), "", "", "", "", ""});
                continue;
            }
            const qint64 due = idOf(self->m_hw, "due_at");
            QString status = s["reviewed_at"].isNull() ? T("待批改") : T("已批改");
            if (due > 0 && idOf(s, "submitted_at") > due) status += T("（迟交）");
            const QString result = ts ? QStringLiteral("%1 / %2").arg(s["solved"].toInt()).arg(s["total"].toInt())
                                      : T("%1 盘").arg(s["game_ids"].toArray().size());
            setRow(self->m_table, i, {row["student_name"].toString(), status, Online::formatTime(idOf(s, "submitted_at")), result,
                                      s["score"].isNull() ? QString() : QString::number(s["score"].toInt()),
                                      s["note"].toString(), s["comment"].toString()});
        }
    });
}

void SubmissionsDialog::openGame()
{
    const QJsonObject s = selectedSubmission();
    const QJsonArray ids = s["game_ids"].toArray();
    if (ids.isEmpty()) return;
    qint64 id = qint64(ids.last().toDouble());
    if (ids.size() > 1) {
        QStringList items;
        for (int i = 0; i < ids.size(); ++i) items << T("第 %1 盘").arg(i + 1);
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, T("打开棋谱"), T("打开哪一盘："), items, items.size() - 1, false, &ok);
        if (!ok) return;
        id = qint64(ids[items.indexOf(pick)].toDouble());
    }
    QPointer<SubmissionsDialog> self(this);
    api()->get(QStringLiteral("/games/%1").arg(id), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("打开棋谱"), r); return; }
        const QJsonObject g = r.data.toObject();
        self->m_host->loadSgfText(g["sgf"].toString(), g["title"].toString());
    });
}

void SubmissionsDialog::review()
{
    const QJsonObject s = selectedSubmission();
    if (s.isEmpty()) return;
    QDialog dlg(this);
    dlg.setWindowTitle(T("批改"));
    auto *form = new QFormLayout(&dlg);
    auto *useScore = new QCheckBox(T("打分"), &dlg);
    auto *score = new QSpinBox(&dlg);
    score->setRange(0, 100);
    score->setSuffix(T(" 分"));
    useScore->setChecked(!s["score"].isNull());
    score->setValue(s["score"].isNull() ? 90 : s["score"].toInt());
    score->setEnabled(useScore->isChecked());
    connect(useScore, &QCheckBox::toggled, score, &QWidget::setEnabled);
    auto *scoreRow = new QHBoxLayout;
    scoreRow->addWidget(useScore);
    scoreRow->addWidget(score, 1);
    form->addRow(scoreRow);
    auto *comment = new QPlainTextEdit(s["comment"].toString(), &dlg);
    form->addRow(T("评语："), comment);
    form->addRow(okCancel(&dlg));
    if (dlg.exec() != QDialog::Accepted) return;
    QJsonObject body{{"comment", comment->toPlainText()}};
    if (useScore->isChecked()) body["score"] = score->value();
    QPointer<SubmissionsDialog> self(this);
    api()->put(QStringLiteral("/submissions/%1/review").arg(idOf(s)), body, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("批改"), r);
        self->reload();
    });
}

// ---- 管理 ----

AdminDialog::AdminDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(T("管理"));
    auto *lay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildUsersTab(), T("账号"));
    tabs->addTab(buildClassesTab(), T("班级"));
    tabs->addTab(buildAuditTab(), T("操作日志"));
    lay->addWidget(tabs);
    auto *closeBtn = new QPushButton(T("关闭"), this);
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(closeBtn);
    lay->addLayout(row);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int i) {
        if (i == 1) reloadClasses();
        if (i == 2) reloadAudit();
    });
    resize(860, 540);
    reloadUsers();
}

QWidget *AdminDialog::buildUsersTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    m_users = makeTable({T("用户名"), T("名字"), T("角色"), T("棋力"), T("状态"), T("未成年"), T("监护人"), T("开通时间")}, w);
    lay->addWidget(m_users, 1);
    auto *btns = new QHBoxLayout;
    auto *add = new QPushButton(T("新建账号…"), w);
    auto *edit = new QPushButton(T("修改…"), w);
    auto *reset = new QPushButton(T("重置密码…"), w);
    auto *toggle = new QPushButton(T("停用 / 启用"), w);
    for (QPushButton *b : {add, edit, reset, toggle}) btns->addWidget(b);
    btns->addStretch();
    lay->addLayout(btns);
    connect(add, &QPushButton::clicked, this, [this] { editUser(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editUser(false); });
    connect(m_users, &QTableWidget::cellDoubleClicked, this, [this] { editUser(false); });
    connect(reset, &QPushButton::clicked, this, &AdminDialog::resetPassword);
    connect(toggle, &QPushButton::clicked, this, &AdminDialog::toggleUser);
    return w;
}

QWidget *AdminDialog::buildClassesTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QHBoxLayout(w);
    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(T("班级"), w));
    m_classes = new QListWidget(w);
    left->addWidget(m_classes, 1);
    auto *cb = new QHBoxLayout;
    auto *addC = new QPushButton(T("新建班级…"), w);
    auto *editC = new QPushButton(T("改名 / 换老师…"), w);
    auto *delC = new QPushButton(T("删除班级"), w);
    for (QPushButton *b : {addC, editC, delC}) cb->addWidget(b);
    left->addLayout(cb);
    auto *right = new QVBoxLayout;
    right->addWidget(new QLabel(T("学生"), w));
    m_members = new QListWidget(w);
    right->addWidget(m_members, 1);
    auto *mb = new QHBoxLayout;
    auto *addM = new QPushButton(T("加学生…"), w);
    auto *delM = new QPushButton(T("移出班级"), w);
    auto *moveM = new QPushButton(T("调班…"), w);
    mb->addWidget(addM);
    mb->addWidget(moveM);
    mb->addWidget(delM);
    mb->addStretch();
    right->addLayout(mb);
    lay->addLayout(left, 1);
    lay->addLayout(right, 1);
    connect(m_classes, &QListWidget::currentRowChanged, this, &AdminDialog::reloadMembers);
    connect(addC, &QPushButton::clicked, this, [this] { editClass(true); });
    connect(editC, &QPushButton::clicked, this, [this] { editClass(false); });
    connect(delC, &QPushButton::clicked, this, &AdminDialog::deleteClass);
    connect(addM, &QPushButton::clicked, this, &AdminDialog::addMember);
    connect(delM, &QPushButton::clicked, this, &AdminDialog::removeMember);
    connect(moveM, &QPushButton::clicked, this, &AdminDialog::moveMember);
    return w;
}

QWidget *AdminDialog::buildAuditTab()
{
    m_audit = makeTable({T("时间"), T("操作人"), T("操作"), T("对象"), T("详情")}, this);
    return m_audit;
}

QJsonObject AdminDialog::selectedUser() const
{
    const int row = m_users->currentRow();
    return row >= 0 && row < m_userList.size() ? m_userList[row].toObject() : QJsonObject();
}

QJsonObject AdminDialog::selectedClass() const
{
    const int row = m_classes->currentRow();
    return row >= 0 && row < m_classList.size() ? m_classList[row].toObject() : QJsonObject();
}

void AdminDialog::reloadUsers()
{
    QPointer<AdminDialog> self(this);
    api()->get("/users", [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("管理"), r); return; }
        self->m_userList = r.data.toArray();
        self->m_users->setRowCount(self->m_userList.size());
        for (int i = 0; i < self->m_userList.size(); ++i) {
            const QJsonObject u = self->m_userList[i].toObject();
            setRow(self->m_users, i, {u["username"].toString(), u["display_name"].toString(), ApiClient::roleName(u["role"].toString()),
                                      Online::rankName(u["rank"].toInt()), u["status"].toString() == "disabled" ? T("已停用") : T("正常"),
                                      u["is_minor"].toBool() ? T("是") : QString(),
                                      u["guardian_name"].toString(), Online::formatTime(idOf(u, "created_at"))});
        }
    });
}

void AdminDialog::editUser(bool create)
{
    const QJsonObject u = create ? QJsonObject() : selectedUser();
    if (!create && u.isEmpty()) return;
    QDialog dlg(this);
    dlg.setWindowTitle(create ? T("新建账号") : T("修改账号：%1").arg(u["username"].toString()));
    auto *form = new QFormLayout(&dlg);
    auto *username = new QLineEdit(u["username"].toString(), &dlg);
    username->setEnabled(create);
    username->setPlaceholderText(T("小写字母、数字，3 到 32 位，以后不能改"));
    auto *display = new QLineEdit(u["display_name"].toString(), &dlg);
    auto *role = new QComboBox(&dlg);
    for (const char *r : {"student", "teacher", "admin"}) role->addItem(ApiClient::roleName(r), r);
    role->setCurrentIndex(qMax(0, role->findData(u["role"].toString("student"))));
    auto *rank = new QComboBox(&dlg);
    Online::fillRankBox(rank, u["rank"].toInt());
    auto *password = new QLineEdit(&dlg);
    auto *minor = new QCheckBox(T("未成年人"), &dlg);
    minor->setChecked(u["is_minor"].toBool());
    auto *gName = new QLineEdit(u["guardian_name"].toString(), &dlg);
    auto *gContact = new QLineEdit(u["guardian_contact"].toString(), &dlg);
    auto *consent = new QCheckBox(T("已取得监护人同意（收集和使用孩子的个人信息）"), &dlg);
    consent->setChecked(u["guardian_consent"].toBool());
    form->addRow(T("用户名："), username);
    form->addRow(T("名字："), display);
    form->addRow(T("角色："), role);
    form->addRow(T("棋力："), rank);
    if (create) {
        password->setPlaceholderText(T("至少 6 位"));
        form->addRow(T("初始密码："), password);
    }
    form->addRow(minor);
    form->addRow(T("监护人："), gName);
    form->addRow(T("监护人联系方式："), gContact);
    form->addRow(consent);
    auto syncGuardian = [=] {
        for (QWidget *x : {static_cast<QWidget *>(gName), static_cast<QWidget *>(gContact), static_cast<QWidget *>(consent)})
            x->setEnabled(minor->isChecked());
    };
    connect(minor, &QCheckBox::toggled, &dlg, syncGuardian);
    syncGuardian();
    form->addRow(okCancel(&dlg));
    if (role->currentData().toString() != "student" && !create) role->setToolTip(T("改角色后该账号需要重新登录"));
    if (dlg.exec() != QDialog::Accepted) return;

    QJsonObject body{{"display_name", display->text()}, {"role", role->currentData().toString()}, {"rank", rank->currentData().toInt()},
                     {"is_minor", minor->isChecked()},
                     {"guardian_name", gName->text()}, {"guardian_contact", gContact->text()}, {"guardian_consent", consent->isChecked()}};
    QPointer<AdminDialog> self(this);
    auto done = [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadUsers();
    };
    if (create) {
        body["username"] = username->text();
        body["password"] = password->text();
        api()->post("/users", body, done);
    } else {
        if (body["role"] == u["role"]) body.remove("role");
        api()->patch(QStringLiteral("/users/%1").arg(idOf(u)), body, done);
    }
}

void AdminDialog::toggleUser()
{
    const QJsonObject u = selectedUser();
    if (u.isEmpty()) return;
    const bool disable = u["status"].toString() != "disabled";
    if (disable && QMessageBox::question(this, T("停用账号"), T("停用 %1？停用后这个账号不能登录，已登录的会被退出。").arg(u["display_name"].toString()))
                       != QMessageBox::Yes)
        return;
    QPointer<AdminDialog> self(this);
    api()->patch(QStringLiteral("/users/%1").arg(idOf(u)), QJsonObject{{"status", disable ? "disabled" : "active"}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadUsers();
    });
}

void AdminDialog::resetPassword()
{
    const QJsonObject u = selectedUser();
    if (u.isEmpty()) return;
    bool ok = false;
    const QString pw = QInputDialog::getText(this, T("重置密码"), T("给 %1 设一个新密码（至少 6 位）：").arg(u["display_name"].toString()),
                                             QLineEdit::Normal, QString(), &ok);
    if (!ok || pw.isEmpty()) return;
    QPointer<AdminDialog> self(this);
    api()->patch(QStringLiteral("/users/%1").arg(idOf(u)), QJsonObject{{"password", pw}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("重置密码"), r);
        else QMessageBox::information(self, T("重置密码"), T("密码已重置，请把新密码告诉对方。"));
    });
}

void AdminDialog::reloadClasses()
{
    const qint64 keep = idOf(selectedClass());
    QPointer<AdminDialog> self(this);
    api()->get("/classes", [self, keep](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("管理"), r); return; }
        self->m_classList = r.data.toArray();
        self->m_classes->blockSignals(true);
        self->m_classes->clear();
        int select = self->m_classList.isEmpty() ? -1 : 0;
        for (int i = 0; i < self->m_classList.size(); ++i) {
            const QJsonObject c = self->m_classList[i].toObject();
            if (idOf(c) == keep) select = i;
            self->m_classes->addItem(T("%1　老师：%2　%3 人").arg(c["name"].toString(), c["teacher_name"].toString()).arg(c["students"].toInt()));
        }
        self->m_classes->blockSignals(false);
        self->m_classes->setCurrentRow(select);
        self->reloadMembers();
    });
}

void AdminDialog::reloadMembers()
{
    const QJsonObject c = selectedClass();
    m_members->clear();
    m_memberList = {};
    if (c.isEmpty()) return;
    QPointer<AdminDialog> self(this);
    api()->get(QStringLiteral("/classes/%1/members").arg(idOf(c)), [self](const ApiResult &r) {
        if (!self || !r.ok) return;
        self->m_memberList = r.data.toArray();
        self->m_members->clear();
        for (const QJsonValue &v : self->m_memberList)
            self->m_members->addItem(T("%1（%2）").arg(v["display_name"].toString(), v["username"].toString())
                                     + QStringLiteral("  ·  ") + Online::rankName(v["rank"].toInt()));
    });
}

void AdminDialog::editClass(bool create)
{
    const QJsonObject c = create ? QJsonObject() : selectedClass();
    if (!create && c.isEmpty()) return;
    QDialog dlg(this);
    dlg.setWindowTitle(create ? T("新建班级") : T("修改班级"));
    auto *form = new QFormLayout(&dlg);
    auto *name = new QLineEdit(c["name"].toString(), &dlg);
    auto *teacher = new QComboBox(&dlg);
    for (const QJsonValue &v : m_userList) {
        const QJsonObject u = v.toObject();
        if (u["role"].toString() == "student" || u["status"].toString() == "disabled") continue;
        teacher->addItem(T("%1（%2）").arg(u["display_name"].toString(), u["username"].toString()), idOf(u));
    }
    teacher->setCurrentIndex(qMax(0, teacher->findData(idOf(c, "teacher_id"))));
    form->addRow(T("班级名称："), name);
    form->addRow(T("任课老师："), teacher);
    form->addRow(okCancel(&dlg));
    if (dlg.exec() != QDialog::Accepted || teacher->count() == 0) return;
    const QJsonObject body{{"name", name->text()}, {"teacher_id", teacher->currentData().toLongLong()}};
    QPointer<AdminDialog> self(this);
    auto done = [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadClasses();
    };
    if (create) api()->post("/classes", body, done);
    else api()->patch(QStringLiteral("/classes/%1").arg(idOf(c)), body, done);
}

void AdminDialog::deleteClass()
{
    const QJsonObject c = selectedClass();
    if (c.isEmpty()) return;
    if (QMessageBox::question(this, T("删除班级"), T("删除班级“%1”？这个班布置过的作业和学生交的内容会一起删除，学生账号和棋谱保留。").arg(c["name"].toString()))
        != QMessageBox::Yes)
        return;
    QPointer<AdminDialog> self(this);
    api()->del(QStringLiteral("/classes/%1").arg(idOf(c)), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadClasses();
    });
}

void AdminDialog::addMember()
{
    const QJsonObject c = selectedClass();
    if (c.isEmpty()) return;
    QStringList items;
    QList<qint64> ids;
    for (const QJsonValue &v : m_userList) {
        const QJsonObject u = v.toObject();
        if (u["role"].toString() != "student") continue;
        bool already = false;
        for (const QJsonValue &m : m_memberList) already = already || idOf(m.toObject()) == idOf(u);
        if (already) continue;
        items << T("%1（%2）").arg(u["display_name"].toString(), u["username"].toString());
        ids << idOf(u);
    }
    if (items.isEmpty()) { QMessageBox::information(this, T("加学生…"), T("没有可加的学生账号。请先在“账号”里新建学生。")); return; }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, T("加学生…"), T("把谁加进“%1”：").arg(c["name"].toString()), items, 0, false, &ok);
    if (!ok) return;
    QPointer<AdminDialog> self(this);
    api()->post(QStringLiteral("/classes/%1/members").arg(idOf(c)), QJsonObject{{"student_id", ids[items.indexOf(pick)]}}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadClasses();
    });
}

void AdminDialog::removeMember()
{
    const QJsonObject c = selectedClass();
    const int row = m_members->currentRow();
    if (c.isEmpty() || row < 0 || row >= m_memberList.size()) return;
    const QJsonObject u = m_memberList[row].toObject();
    QPointer<AdminDialog> self(this);
    api()->del(QStringLiteral("/classes/%1/members/%2").arg(idOf(c)).arg(idOf(u)), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) Online::showError(self, T("管理"), r);
        self->reloadClasses();
    });
}

void AdminDialog::moveMember()
{
    const QJsonObject c = selectedClass();
    const int row = m_members->currentRow();
    if (c.isEmpty() || row < 0 || row >= m_memberList.size()) {
        QMessageBox::information(this, T("调班"), T("请先在右边选中要调班的学生。"));
        return;
    }
    const QJsonObject u = m_memberList[row].toObject();
    QStringList items;
    QList<qint64> ids;
    for (const QJsonValue &v : m_classList) {
        const QJsonObject o = v.toObject();
        if (idOf(o) == idOf(c)) continue;
        items << T("%1　老师：%2　%3 人").arg(o["name"].toString(), o["teacher_name"].toString()).arg(o["students"].toInt());
        ids << idOf(o);
    }
    if (items.isEmpty()) { QMessageBox::information(this, T("调班"), T("没有别的班可以调。请先新建班级。")); return; }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, T("调班"), T("把 %1 从“%2”调到：").arg(u["display_name"].toString(), c["name"].toString()),
                                               items, 0, false, &ok);
    if (!ok) return;
    QPointer<AdminDialog> self(this);
    api()->post(QStringLiteral("/classes/%1/members/%2/move").arg(idOf(c)).arg(idOf(u)),
                QJsonObject{{"to_class_id", ids[items.indexOf(pick)]}}, [self](const ApiResult &r) {
                    if (!self) return;
                    if (!r.ok) Online::showError(self, T("调班"), r);
                    self->reloadClasses();
                });
}

void AdminDialog::reloadAudit()
{
    QPointer<AdminDialog> self(this);
    api()->get("/audit", [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, T("管理"), r); return; }
        const QJsonArray list = r.data.toArray();
        self->m_audit->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            const QJsonObject e = list[i].toObject();
            setRow(self->m_audit, i, {Online::formatTime(idOf(e, "created_at")), e["actor"].toString(), e["action"].toString(),
                                      e["target"].toString(), e["detail"].toString()});
        }
    });
}
