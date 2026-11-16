#include "I18n.h"
#include "SettingsDialog.h"
#include "Rules.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

static QWidget *withBrowse(QLineEdit *edit, const QString &title, const QString &filter, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->addWidget(edit, 1);
    auto *btn = new QPushButton(T("浏览…"), w);
    h->addWidget(btn);
    QObject::connect(btn, &QPushButton::clicked, w, [=] {
        QString start = edit->text().isEmpty() ? QString() : QFileInfo(edit->text()).absolutePath();
        QString f = QFileDialog::getOpenFileName(parent, title, start, filter);
        if (!f.isEmpty()) edit->setText(f);
    });
    return w;
}

SettingsDialog::SettingsDialog(const AppSettings &s, const QStringList &devices, QWidget *parent)
    : QDialog(parent), m_s(s)
{
    setWindowTitle(T("设置"));
    setMinimumWidth(620);
    auto *form = new QFormLayout(this);

    m_katago = new QLineEdit(s.katagoPath, this);
    form->addRow(T("KataGo 程序："), withBrowse(m_katago, T("选择 KataGo 程序"), T("KataGo (*.exe);;所有文件 (*)"), this));
    auto *tip = new QLabel(T("有独立显卡建议用 OpenCL 版（文件名含 opencl）；没有独立显卡用 CPU 版（文件名含 eigen）。"), this);
    tip->setStyleSheet("color: gray;");
    form->addRow(QString(), tip);

    m_model = new QLineEdit(s.modelPath, this);
    form->addRow(T("模型文件："), withBrowse(m_model, T("选择模型"), T("KataGo 模型 (*.bin.gz *.txt.gz)"), this));

    m_gpu = new QComboBox(this);
    m_gpu->addItem(T("自动（推荐）"), -1);
    for (const QString &d : devices) m_gpu->addItem(d, d.section(':', 0, 0).toInt());
    if (devices.isEmpty()) for (int i = 0; i < 3; ++i) m_gpu->addItem(T("%1 号显卡").arg(i), i);
    int idx = m_gpu->findData(s.gpuDevice);
    m_gpu->setCurrentIndex(idx >= 0 ? idx : 0);
    form->addRow(T("显卡："), m_gpu);

    m_threads = new QSpinBox(this);
    m_threads->setRange(0, 128);
    m_threads->setSpecialValueText(s.tunedThreads > 0 && s.tunedFor == s.tuneKey()
                                       ? T("自动（本机测试结果：%1）").arg(s.tunedThreads)
                                       : T("自动（首次启动时测试）"));
    m_threads->setValue(s.searchThreads);
    auto *retune = new QPushButton(T("重新测试硬件"), this);
    connect(retune, &QPushButton::clicked, this, [this, retune] {
        m_s.tunedThreads = 0;
        m_s.tunedFor.clear();
        m_threads->setValue(0);
        m_threads->setSpecialValueText(T("自动（确定后重新测试）"));
        retune->setEnabled(false);
    });
    auto *tw = new QWidget(this);
    auto *th = new QHBoxLayout(tw);
    th->setContentsMargins(0, 0, 0, 0);
    th->addWidget(m_threads, 1);
    th->addWidget(retune);
    form->addRow(T("搜索线程数："), tw);

    m_visits = new QSpinBox(this);
    m_visits->setRange(10, 1000000);
    m_visits->setSingleStep(100);
    m_visits->setValue(s.maxVisits);
    form->addRow(T("每步最大计算量："), m_visits);

    m_komi = new QDoubleSpinBox(this);
    m_komi->setRange(-50, 50);
    m_komi->setSingleStep(0.5);
    m_komi->setValue(s.komi);
    form->addRow(T("贴目（新对局）："), m_komi);

    m_rules = new QComboBox(this);
    Rules::fillBox(m_rules, s.rules);
    form->addRow(T("规则（新对局）："), m_rules);
    // 换规则时贴目跟着换成该规则的默认值（之后还可以手动改）
    connect(m_rules, &QComboBox::currentIndexChanged, this, [this] {
        m_komi->setValue(Rules::get(m_rules->currentData().toString()).komi);
    });

    // 语言名称用各自的文字显示，任何语言下都看得懂
    m_lang = new QComboBox(this);
    m_lang->addItem(T("自动 / Auto"), "auto");
    m_lang->addItem(QString::fromUtf8("中文"), "zh");
    m_lang->addItem("English", "en");
    m_lang->addItem(QString::fromUtf8("日本語"), "ja");
    m_lang->addItem(QString::fromUtf8("한국어"), "ko");
    m_lang->setCurrentIndex(qMax(0, m_lang->findData(s.language)));
    form->addRow(T("界面语言："), m_lang);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(T("确定"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(bb);
}

AppSettings SettingsDialog::result() const
{
    AppSettings s = m_s;
    s.katagoPath = m_katago->text().trimmed();
    s.modelPath = m_model->text().trimmed();
    s.gpuDevice = m_gpu->currentData().toInt();
    s.searchThreads = m_threads->value();
    s.maxVisits = m_visits->value();
    s.komi = m_komi->value();
    s.rules = m_rules->currentData().toString();
    s.language = m_lang->currentData().toString();
    return s;
}
