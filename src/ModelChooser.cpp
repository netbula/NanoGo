#include "ModelChooser.h"
#include "AppSettings.h"
#include "I18n.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

static QString fileNameOf(const NetworkInfo &n) { return QFileInfo(QUrl(n.url).path()).fileName(); }

ModelChooser::ModelChooser(const QList<NetworkInfo> &official, const QString &currentPath, QWidget *parent)
    : QDialog(parent), m_official(official), m_current(QFileInfo(currentPath).fileName())
{
    setWindowTitle(T("选择模型"));
    resize(860, 520);
    auto *v = new QVBoxLayout(this);
    auto *tip = new QLabel(T("默认用官方最强的模型。也可以选别的：小模型（文件小）算得快，适合没有独立显卡的电脑；"
                             "没下载过的模型会先下载再切换。手动选择后会关闭“自动更新到最强模型”。"), this);
    tip->setWordWrap(true);
    v->addWidget(tip);
    m_onlyRated = new QCheckBox(T("只显示评测充分的模型（至少 1000 局）"), this);
    m_onlyRated->setChecked(true);
    v->addWidget(m_onlyRated);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({T("模型"), T("等级分"), T("评测局数"), T("大小"), T("日期"), T("状态")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    v->addWidget(m_table, 1);

    auto *bb = new QDialogButtonBox(this);
    auto *useBtn = bb->addButton(T("使用这个模型"), QDialogButtonBox::AcceptRole);
    auto *fileBtn = bb->addButton(T("从文件选择…"), QDialogButtonBox::ActionRole);
    bb->addButton(T("取消"), QDialogButtonBox::RejectRole);
    v->addWidget(bb);
    connect(useBtn, &QPushButton::clicked, this, &ModelChooser::acceptRow);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ModelChooser::acceptRow);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(fileBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, T("选择模型"), AppSettings::modelsDir(),
                                                       T("KataGo 模型 (*.bin.gz *.txt.gz)"));
        if (f.isEmpty()) return;
        m_localPath = f;
        accept();
    });
    connect(m_onlyRated, &QCheckBox::toggled, this, &ModelChooser::fill);
    fill();
}

void ModelChooser::fill()
{
    m_table->setRowCount(0);
    const NetworkInfo best = ModelUpdater::strongest(m_official);
    QDir models(AppSettings::modelsDir());
    QStringList localFiles = models.entryList({"*.bin.gz", "*.txt.gz"}, QDir::Files);

    QList<NetworkInfo> rows = m_official;
    std::sort(rows.begin(), rows.end(), [](const NetworkInfo &a, const NetworkInfo &b) { return a.eloLower > b.eloLower; });

    auto addRow = [this](const QStringList &cells, const QVariant &data, bool current) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        for (int c = 0; c < cells.size(); ++c) {
            auto *it = new QTableWidgetItem(cells[c]);
            if (c == 0) it->setData(Qt::UserRole, data);
            if (current) { QFont f = it->font(); f.setBold(true); it->setFont(f); }
            m_table->setItem(r, c, it);
        }
        if (current) m_table->selectRow(r);
    };

    // 本机已有、但不在官方列表里的模型
    for (const QString &f : localFiles) {
        bool inList = false;
        for (const NetworkInfo &n : m_official) if (fileNameOf(n) == f) { inList = true; break; }
        if (inList) continue;
        const qint64 mb = QFileInfo(models.filePath(f)).size() >> 20;
        addRow({f, "-", "-", QStringLiteral("%1 MB").arg(mb), "-", f == m_current ? T("正在使用") : T("本机文件")},
               QStringLiteral("local:") + models.filePath(f), f == m_current);
    }
    for (int i = 0; i < rows.size(); ++i) {
        const NetworkInfo &n = rows[i];
        const QString file = fileNameOf(n);
        const bool current = file == m_current;
        if (m_onlyRated->isChecked() && n.games < 1000 && !current) continue;
        QString state = current ? T("正在使用") : localFiles.contains(file) ? T("已下载") : T("需要下载");
        QString name = n.name;
        if (n.name == best.name) name = T("★ %1（最强，默认）").arg(n.name);
        int idx = -1;
        for (int k = 0; k < m_official.size(); ++k) if (m_official[k].name == n.name) { idx = k; break; }
        addRow({name, QStringLiteral("%1 ± %2").arg(n.elo, 0, 'f', 0).arg(n.eloUncertainty, 0, 'f', 0),
                QString::number(n.games), QStringLiteral("%1 MB").arg(n.bytes >> 20), n.created.left(10), state},
               idx, current);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void ModelChooser::acceptRow()
{
    const int r = m_table->currentRow();
    if (r < 0) return;
    const QVariant d = m_table->item(r, 0)->data(Qt::UserRole);
    if (d.typeId() == QMetaType::QString && d.toString().startsWith("local:")) {
        m_localPath = d.toString().mid(6);
    } else {
        const int idx = d.toInt();
        if (idx < 0 || idx >= m_official.size()) return;
        m_network = m_official[idx];
        const QString local = QDir(AppSettings::modelsDir()).filePath(fileNameOf(m_network));
        if (QFileInfo::exists(local)) { m_localPath = local; m_network = {}; }
    }
    accept();
}
