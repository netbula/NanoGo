#include "SettingsDialog.h"
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
    auto *btn = new QPushButton(QStringLiteral("浏览…"), w);
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
    setWindowTitle(QStringLiteral("设置"));
    setMinimumWidth(620);
    auto *form = new QFormLayout(this);

    m_katago = new QLineEdit(s.katagoPath, this);
    form->addRow(QStringLiteral("KataGo 程序："), withBrowse(m_katago, QStringLiteral("选择 KataGo 程序"), "KataGo (*.exe);;所有文件 (*)", this));
    auto *tip = new QLabel(QStringLiteral("建议用 OpenCL 版（文件名含 opencl），你的 5060 Ti 已测试可用。"), this);
    tip->setStyleSheet("color: gray;");
    form->addRow(QString(), tip);

    m_model = new QLineEdit(s.modelPath, this);
    form->addRow(QStringLiteral("模型文件："), withBrowse(m_model, QStringLiteral("选择模型"), QStringLiteral("KataGo 模型 (*.bin.gz *.txt.gz)"), this));

    m_gpu = new QComboBox(this);
    m_gpu->addItem(QStringLiteral("自动（推荐）"), -1);
    for (const QString &d : devices) m_gpu->addItem(d, d.section(':', 0, 0).toInt());
    if (devices.isEmpty()) for (int i = 0; i < 3; ++i) m_gpu->addItem(QStringLiteral("%1 号显卡").arg(i), i);
    int idx = m_gpu->findData(s.gpuDevice);
    m_gpu->setCurrentIndex(idx >= 0 ? idx : 0);
    form->addRow(QStringLiteral("显卡："), m_gpu);

    m_threads = new QSpinBox(this);
    m_threads->setRange(1, 128);
    m_threads->setValue(s.searchThreads);
    form->addRow(QStringLiteral("搜索线程数："), m_threads);

    m_visits = new QSpinBox(this);
    m_visits->setRange(10, 1000000);
    m_visits->setSingleStep(100);
    m_visits->setValue(s.maxVisits);
    form->addRow(QStringLiteral("每步最大计算量："), m_visits);

    m_komi = new QDoubleSpinBox(this);
    m_komi->setRange(-50, 50);
    m_komi->setSingleStep(0.5);
    m_komi->setValue(s.komi);
    form->addRow(QStringLiteral("贴目（新对局）："), m_komi);

    m_rules = new QComboBox(this);
    m_rules->addItem(QStringLiteral("中国规则"), "chinese");
    m_rules->addItem(QStringLiteral("日本规则"), "japanese");
    m_rules->addItem(QStringLiteral("韩国规则"), "korean");
    m_rules->addItem(QStringLiteral("AGA 规则"), "aga");
    m_rules->addItem(QStringLiteral("Tromp-Taylor"), "tromp-taylor");
    m_rules->setCurrentIndex(qMax(0, m_rules->findData(s.rules)));
    form->addRow(QStringLiteral("规则（新对局）："), m_rules);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
    return s;
}
