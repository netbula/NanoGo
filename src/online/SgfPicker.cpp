#include "SgfPicker.h"
#include "ApiClient.h"
#include "OnlineDialogs.h"
#include "../AppSettings.h"
#include "../BoardWidget.h"
#include "../I18n.h"
#include "../Sgf.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
ApiClient *api() { return ApiClient::instance(); }
qint64 idOf(const QJsonObject &o) { return qint64(o.value("id").toDouble()); }

// 记住最近用过的本地文件和文件夹，下次打开选择框直接列出来
QSettings &pickerStore()
{
    static QSettings s(QDir(AppSettings::dataDir()).filePath("picker.ini"), QSettings::IniFormat);
    return s;
}
QStringList recent(const QString &key)
{
    QStringList list;
    for (const QString &p : pickerStore().value(key).toStringList())
        if (QFileInfo::exists(p)) list << p;
    return list;
}
void remember(const QString &key, const QString &path)
{
    QStringList list = pickerStore().value(key).toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 10) list.removeLast();
    pickerStore().setValue(key, list);
    pickerStore().setValue("lastDir", QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath());
}
QString lastDir() { return pickerStore().value("lastDir").toString(); }

QString readSgfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return Sgf::decode(f.readAll());
}

BoardWidget *previewBoard(GameTree *tree, QWidget *parent)
{
    auto *b = new BoardWidget(tree, parent);
    b->setMinimumSize(280, 280);
    b->setAttribute(Qt::WA_TransparentForMouseEvents);  // 只看不下
    return b;
}

QDialogButtonBox *buttons(QDialog *dlg, QPushButton **ok)
{
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    *ok = bb->button(QDialogButtonBox::Ok);
    (*ok)->setText(T("选定"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return bb;
}
}

// ---- 死活题 ----

ProblemPickerDialog::ProblemPickerDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(T("选择死活题"));
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(T("题库："), this));
    m_source = new QComboBox(this);
    top->addWidget(m_source, 1);
    auto *fileBtn = new QPushButton(T("打开文件…"), this);
    auto *dirBtn = new QPushButton(T("打开文件夹…"), this);
    top->addWidget(fileBtn);
    top->addWidget(dirBtn);
    lay->addLayout(top);

    auto *split = new QSplitter(this);
    auto *left = new QWidget(split);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(left);
    leftLay->addWidget(m_list, 1);
    auto *selRow = new QHBoxLayout;
    auto *allBtn = new QPushButton(T("全选"), left);
    auto *noneBtn = new QPushButton(T("全不选"), left);
    m_count = new QLabel(left);
    selRow->addWidget(allBtn);
    selRow->addWidget(noneBtn);
    selRow->addStretch();
    selRow->addWidget(m_count);
    leftLay->addLayout(selRow);
    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    m_board = previewBoard(&m_tree, right);
    m_info = new QLabel(right);
    m_info->setAlignment(Qt::AlignCenter);
    rightLay->addWidget(m_board, 1);
    rightLay->addWidget(m_info);
    split->addWidget(left);
    split->addWidget(right);
    split->setSizes({300, 600});
    lay->addWidget(split, 1);
    auto *bb = buttons(this, &m_ok);
    lay->addWidget(bb);

    m_source->addItem(T("自带：基础死活题"), QStringLiteral("builtin"));
    for (const QString &p : recent("problemFiles")) addFileSource(p, false);

    connect(m_source, &QComboBox::currentIndexChanged, this, &ProblemPickerDialog::sourceChanged);
    connect(fileBtn, &QPushButton::clicked, this, &ProblemPickerDialog::openFile);
    connect(dirBtn, &QPushButton::clicked, this, &ProblemPickerDialog::openFolder);
    connect(m_list, &QListWidget::currentRowChanged, this, &ProblemPickerDialog::showPreview);
    connect(m_list, &QListWidget::itemChanged, this, &ProblemPickerDialog::updateCount);
    auto checkAll = [this](Qt::CheckState s) {
        for (int i = 0; i < m_list->count(); ++i) m_list->item(i)->setCheckState(s);
    };
    connect(allBtn, &QPushButton::clicked, this, [checkAll] { checkAll(Qt::Checked); });
    connect(noneBtn, &QPushButton::clicked, this, [checkAll] { checkAll(Qt::Unchecked); });

    // 服务器上已有的题集排在最前面
    QPointer<ProblemPickerDialog> self(this);
    api()->get("/problemsets", [self](const ApiResult &r) {
        if (!self || !r.ok) return;
        const QSignalBlocker block(self->m_source);
        int at = 0;
        for (const QJsonValue &v : r.data.toArray()) {
            const QJsonObject p = v.toObject();
            self->m_source->insertItem(at++, T("服务器：%1（%2 题）").arg(p["title"].toString()).arg(p["problem_count"].toInt()),
                                       QStringLiteral("server:%1").arg(idOf(p)));
        }
        if (at > 0) self->m_source->setCurrentIndex(0);
        self->sourceChanged();
    });
    sourceChanged();
    resize(900, 600);
}

void ProblemPickerDialog::addFileSource(const QString &path, bool select)
{
    const QString key = "file:" + path;
    int i = m_source->findData(key);
    if (i < 0) {
        m_source->addItem(T("文件：%1").arg(QFileInfo(path).fileName()), key);
        i = m_source->count() - 1;
        m_source->setItemData(i, QDir::toNativeSeparators(path), Qt::ToolTipRole);
    }
    if (select) m_source->setCurrentIndex(i);
}

void ProblemPickerDialog::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, T("打开题库"), lastDir(), T("SGF 题目 (*.sgf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    remember("problemFiles", path);
    addFileSource(path, true);
}

void ProblemPickerDialog::openFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, T("打开文件夹…"), lastDir());
    if (dir.isEmpty()) return;
    const QFileInfoList files = QDir(dir).entryInfoList({"*.sgf", "*.SGF"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        m_info->setText(T("这个文件夹里没有 SGF 文件。"));
        return;
    }
    pickerStore().setValue("lastDir", dir);
    const QSignalBlocker block(m_source);
    for (const QFileInfo &f : files) addFileSource(f.absoluteFilePath(), false);
    addFileSource(files.first().absoluteFilePath(), true);
    sourceChanged();
}

void ProblemPickerDialog::sourceChanged()
{
    const QString src = m_source->currentData().toString();
    const QString label = m_source->currentText();
    const int serial = ++m_loadSerial;
    m_setId = 0;
    if (src == "builtin") {
        setProblems(Tsumego::builtin(), T("基础死活题"));
    } else if (src.startsWith("file:")) {
        const QString path = src.mid(5);
        setProblems(Tsumego::loadCollection(readSgfFile(path)), QFileInfo(path).completeBaseName());
    } else if (src.startsWith("server:")) {
        const qint64 id = src.mid(7).toLongLong();
        setProblems({}, QString());
        m_info->setText(T("正在读取…"));
        QPointer<ProblemPickerDialog> self(this);
        api()->get(QStringLiteral("/problemsets/%1").arg(id), [self, serial, id](const ApiResult &r) {
            if (!self || serial != self->m_loadSerial) return;
            if (!r.ok) { self->m_info->setText(r.message()); return; }
            const QJsonObject o = r.data.toObject();
            self->setProblems(Tsumego::loadCollection(o["sgf"].toString()), o["title"].toString());
            self->m_setId = id;
        });
    }
    Q_UNUSED(label);
}

void ProblemPickerDialog::setProblems(const QList<TsumegoProblem> &list, const QString &name)
{
    m_problems = list;
    m_name = name;
    const QSignalBlocker block(m_list);
    m_list->clear();
    for (int i = 0; i < list.size(); ++i) {
        const TsumegoProblem &p = list[i];
        QString text = QStringLiteral("%1. ").arg(i + 1);
        if (!p.title.isEmpty()) text += p.title + "  ";
        text += Tsumego::goalText(p);
        auto *it = new QListWidgetItem(text, m_list);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
    }
    if (!list.isEmpty()) m_list->setCurrentRow(0);
    else {
        Sgf::load(QStringLiteral("(;SZ[19])"), m_tree, nullptr);
        m_board->setViewRect({});
        m_board->update();
        m_info->setText(name.isEmpty() ? QString() : T("这个文件里没有找到死活题（需要有摆好的黑白子）。"));
    }
    updateCount();
}

void ProblemPickerDialog::showPreview()
{
    const int i = m_list->currentRow();
    if (i < 0 || i >= m_problems.size()) return;
    const TsumegoProblem &p = m_problems[i];
    Sgf::load(Tsumego::toSgf(p), m_tree, nullptr);
    m_board->setViewRect(Tsumego::region(p, 2));
    m_board->update();
    m_info->setText(QStringLiteral("%1 / %2　%3").arg(i + 1).arg(m_problems.size()).arg(Tsumego::goalText(p)));
}

void ProblemPickerDialog::updateCount()
{
    const int n = problems().size();
    m_count->setText(T("已选 %1 / %2 题").arg(n).arg(m_problems.size()));
    m_ok->setEnabled(n > 0);
}

QList<TsumegoProblem> ProblemPickerDialog::problems() const
{
    QList<TsumegoProblem> out;
    for (int i = 0; i < m_list->count() && i < m_problems.size(); ++i)
        if (m_list->item(i)->checkState() == Qt::Checked) out << m_problems[i];
    return out;
}

bool ProblemPickerDialog::allChecked() const { return problems().size() == m_problems.size(); }

qint64 ProblemPickerDialog::serverSetId() const { return allChecked() ? m_setId : 0; }

// ---- 棋谱 ----

GamePickerDialog::GamePickerDialog(OnlineHost *host, QWidget *parent) : QDialog(parent), m_host(host)
{
    setWindowTitle(T("选择棋谱"));
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(T("来源："), this));
    m_source = new QComboBox(this);
    top->addWidget(m_source, 1);
    auto *fileBtn = new QPushButton(T("打开文件…"), this);
    auto *dirBtn = new QPushButton(T("打开文件夹…"), this);
    top->addWidget(fileBtn);
    top->addWidget(dirBtn);
    lay->addLayout(top);

    auto *split = new QSplitter(this);
    m_list = new QListWidget(split);
    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    m_board = previewBoard(&m_tree, right);
    m_slider = new QSlider(Qt::Horizontal, right);
    m_info = new QLabel(right);
    m_info->setAlignment(Qt::AlignCenter);
    m_info->setWordWrap(true);
    rightLay->addWidget(m_board, 1);
    rightLay->addWidget(m_slider);
    rightLay->addWidget(m_info);
    split->addWidget(m_list);
    split->addWidget(right);
    split->setSizes({340, 560});
    lay->addWidget(split, 1);
    lay->addWidget(buttons(this, &m_ok));

    m_source->addItem(T("当前棋盘"), QStringLiteral("current"));
    m_source->addItem(T("云棋谱：我自己"), QStringLiteral("cloud:%1").arg(api()->myId()));
    for (const QString &p : recent("gameSources")) addLocalSource(p, false);

    connect(m_source, &QComboBox::currentIndexChanged, this, &GamePickerDialog::sourceChanged);
    connect(fileBtn, &QPushButton::clicked, this, &GamePickerDialog::openFile);
    connect(dirBtn, &QPushButton::clicked, this, &GamePickerDialog::openFolder);
    connect(m_list, &QListWidget::currentRowChanged, this, &GamePickerDialog::selectionChanged);
    connect(m_slider, &QSlider::valueChanged, this, &GamePickerDialog::showMove);

    // 老师还可以从学生的云棋谱里挑；管理员可以挑所有人的
    if (api()->isTeacher()) {
        QPointer<GamePickerDialog> self(this);
        api()->get("/users", [self](const ApiResult &r) {
            if (!self || !r.ok) return;
            const QSignalBlocker block(self->m_source);
            int at = 2;
            for (const QJsonValue &v : r.data.toArray()) {
                const QJsonObject u = v.toObject();
                if (idOf(u) == api()->myId()) continue;
                self->m_source->insertItem(at++, T("云棋谱：%1").arg(u["display_name"].toString()),
                                           QStringLiteral("cloud:%1").arg(idOf(u)));
            }
        });
    }
    sourceChanged();
    resize(900, 620);
}

void GamePickerDialog::addLocalSource(const QString &path, bool select)
{
    const bool dir = QFileInfo(path).isDir();
    const QString key = (dir ? "dir:" : "file:") + path;
    int i = m_source->findData(key);
    if (i < 0) {
        m_source->addItem((dir ? T("文件夹：%1") : T("文件：%1")).arg(QFileInfo(path).fileName()), key);
        i = m_source->count() - 1;
        m_source->setItemData(i, QDir::toNativeSeparators(path), Qt::ToolTipRole);
    }
    if (select) m_source->setCurrentIndex(i);
}

void GamePickerDialog::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, T("打开棋谱"), lastDir(), T("SGF 棋谱 (*.sgf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    remember("gameSources", path);
    addLocalSource(path, true);
}

void GamePickerDialog::openFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, T("打开文件夹…"), lastDir());
    if (dir.isEmpty()) return;
    remember("gameSources", dir);
    addLocalSource(dir, true);
}

void GamePickerDialog::sourceChanged()
{
    const QString src = m_source->currentData().toString();
    const int serial = ++m_loadSerial;
    {
        const QSignalBlocker block(m_list);
        m_list->clear();
    }
    m_info->clear();
    if (src == "current") {
        auto *it = new QListWidgetItem(T("当前棋盘上的棋谱"), m_list);
        it->setData(Qt::UserRole, QStringLiteral("current"));
    } else if (src.startsWith("file:")) {
        auto *it = new QListWidgetItem(QFileInfo(src.mid(5)).fileName(), m_list);
        it->setData(Qt::UserRole, src);
    } else if (src.startsWith("dir:")) {
        const QFileInfoList files = QDir(src.mid(4)).entryInfoList({"*.sgf", "*.SGF"}, QDir::Files, QDir::Name);
        for (const QFileInfo &f : files) {
            auto *it = new QListWidgetItem(f.fileName(), m_list);
            it->setData(Qt::UserRole, "file:" + f.absoluteFilePath());
        }
        if (files.isEmpty()) m_info->setText(T("这个文件夹里没有 SGF 文件。"));
    } else if (src.startsWith("cloud:")) {
        m_info->setText(T("正在读取…"));
        QPointer<GamePickerDialog> self(this);
        api()->get(QStringLiteral("/games?owner=%1").arg(src.mid(6)), [self, serial](const ApiResult &r) {
            if (!self || serial != self->m_loadSerial) return;
            if (!r.ok) { self->m_info->setText(r.message()); return; }
            const QJsonArray games = r.data.toArray();
            for (const QJsonValue &v : games) {
                const QJsonObject g = v.toObject();
                QString text = g["title"].toString();
                const QString b = g["black"].toString(), w = g["white"].toString();
                if (!b.isEmpty() || !w.isEmpty()) text += QStringLiteral("　%1 vs %2").arg(b, w);
                if (!g["result"].toString().isEmpty()) text += "　" + g["result"].toString();
                auto *it = new QListWidgetItem(text, self->m_list);
                it->setData(Qt::UserRole, QStringLiteral("cloud:%1").arg(idOf(g)));
                it->setData(Qt::UserRole + 1, g["title"].toString());
            }
            self->m_info->setText(games.isEmpty() ? T("还没有云棋谱。") : QString());
            if (!games.isEmpty()) self->m_list->setCurrentRow(0);
        });
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
    else selectionChanged();
}

void GamePickerDialog::selectionChanged()
{
    m_gameId = 0;
    m_sgf.clear();
    m_title.clear();
    QListWidgetItem *it = m_list->currentItem();
    const QString key = it ? it->data(Qt::UserRole).toString() : QString();
    m_ok->setEnabled(false);
    if (key.isEmpty()) { showSgf(QString()); return; }
    const int serial = ++m_loadSerial;
    if (key == "current") {
        m_sgf = m_host->currentGameSgf();
        showSgf(m_sgf);
    } else if (key.startsWith("file:")) {
        m_sgf = readSgfFile(key.mid(5));
        m_title = QFileInfo(key.mid(5)).completeBaseName();
        showSgf(m_sgf);
    } else if (key.startsWith("cloud:")) {
        const qint64 id = key.mid(6).toLongLong();
        m_title = it->data(Qt::UserRole + 1).toString();
        QPointer<GamePickerDialog> self(this);
        api()->get(QStringLiteral("/games/%1").arg(id), [self, serial, id](const ApiResult &r) {
            if (!self || serial != self->m_loadSerial) return;
            if (!r.ok) { self->m_info->setText(r.message()); return; }
            self->m_gameId = id;
            self->showSgf(r.data.toObject()["sgf"].toString());
        });
    }
}

void GamePickerDialog::showSgf(const QString &sgf)
{
    QString err;
    m_tree.playerBlack.clear();
    m_tree.playerWhite.clear();
    m_tree.result.clear();
    const bool ok = !sgf.isEmpty() && Sgf::load(sgf, m_tree, &err);
    if (!ok) Sgf::load(QStringLiteral("(;SZ[19])"), m_tree, nullptr);
    // 主线有多少手
    m_tree.toStart();
    int moves = 0;
    while (m_tree.forward()) ++moves;
    {
        const QSignalBlocker block(m_slider);
        m_slider->setRange(0, moves);
        m_slider->setValue(moves);
    }
    m_board->update();
    if (!ok) {
        m_info->setText(sgf.isEmpty() ? QString() : T("不是有效的 SGF 棋谱。"));
        return;
    }
    QStringList parts;
    if (!m_tree.playerBlack.isEmpty() || !m_tree.playerWhite.isEmpty())
        parts << T("黑：%1　白：%2").arg(m_tree.playerBlack, m_tree.playerWhite);
    if (!m_tree.result.isEmpty()) parts << m_tree.result;
    parts << T("共 %1 手").arg(moves);
    m_info->setText(parts.join("　"));
    if (m_title.isEmpty()) m_title = m_tree.root()->extraProps.value("GN").value(0);
    m_ok->setEnabled(true);
}

void GamePickerDialog::showMove(int n)
{
    m_tree.toStart();
    for (int i = 0; i < n && m_tree.forward(); ++i) {}
    m_board->update();
}
