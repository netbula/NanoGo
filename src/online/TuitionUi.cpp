#include "TuitionUi.h"
#include "ApiClient.h"
#include "OnlineDialogs.h"
#include "../I18n.h"
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace {
ApiClient *api() { return ApiClient::instance(); }
qint64 num(const QJsonValue &v) { return qint64(v.toDouble()); }

QTableWidget *table(const QStringList &headers, QWidget *parent, bool multi)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(multi ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setAlternatingRowColors(true);
    return t;
}

const int kLowLessons = 2;  // 剩这么多课时及以下标红，提醒续费
}

QString Tuition::money(qint64 cents)
{
    const QString s = QLocale(QLocale::English).toString(double(qAbs(cents)) / 100.0, 'f', 2);
    return (cents < 0 ? QStringLiteral("-¥") : QStringLiteral("¥")) + s;
}

QString Tuition::kindName(const QString &kind)
{
    if (kind == QLatin1String("payment")) return T("缴费");
    if (kind == QLatin1String("refund")) return T("退费");
    if (kind == QLatin1String("lesson")) return T("上课");
    if (kind == QLatin1String("adjust")) return T("调整");
    return kind;
}

void Tuition::recordLessons(QWidget *parent, const QList<qint64> &students, qint64 roomId, const QString &note)
{
    if (students.isEmpty()) return;
    QJsonArray ids;
    for (qint64 id : students) ids.append(id);
    QPointer<QWidget> p(parent);
    api()->post(QStringLiteral("/tuition/entries"),
                QJsonObject{{"kind", "lesson"}, {"student_ids", ids}, {"lessons", 1}, {"room_id", roomId}, {"note", note}},
                [p](const ApiResult &r) {
                    if (!p) return;
                    if (!r.ok) Online::showError(p, T("记课时"), r);
                });
}

TuitionDialog::TuitionDialog(QWidget *parent) : QDialog(parent)
{
    const bool admin = api()->isAdmin(), student = api()->isStudent();
    setWindowTitle(student ? T("我的课时") : T("学费和课时"));
    auto *lay = new QVBoxLayout(this);
    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    if (!student) {
        auto *top = new QHBoxLayout;
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText(T("按名字、用户名或班级筛选"));
        m_filter->setClearButtonEnabled(true);
        top->addWidget(m_filter, 1);
        lay->addLayout(top);
        connect(m_filter, &QLineEdit::textChanged, this, &TuitionDialog::fillSummary);
    }
    QStringList heads{T("学生"), T("用户名"), T("班级"), T("剩余课时")};
    if (!api()->isTeacher() || admin) heads << T("累计缴费");
    heads << T("最近上课") << T("最近缴费");
    m_summary = table(heads, this, !student);
    QStringList eheads{T("日期"), T("类型"), T("课时")};
    if (admin || student) eheads << T("金额");
    eheads << T("备注") << T("记录人") << T("状态");
    m_entries = table(eheads, this, false);
    auto *split = new QSplitter(Qt::Vertical, this);
    split->addWidget(m_summary);
    split->addWidget(m_entries);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    if (student) m_summary->setMaximumHeight(80);
    lay->addWidget(split, 1);
    lay->addWidget(m_info);

    auto *btns = new QHBoxLayout;
    auto addBtn = [&](const QString &text, auto fn) {
        auto *b = new QPushButton(text, this);
        btns->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
        return b;
    };
    if (admin) {
        addBtn(T("缴费…"), [this] { addEntry("payment"); });
        addBtn(T("退费…"), [this] { addEntry("refund"); });
        addBtn(T("调整课时…"), [this] { addEntry("adjust"); });
    }
    if (!student) {
        addBtn(T("记上课…"), [this] { addEntry("lesson"); });
        m_void = addBtn(T("作废这一笔…"), [this] { voidEntry(); });
        addBtn(T("导出表格…"), [this] { exportCsv(); });
    }
    btns->addStretch();
    auto *close = new QPushButton(T("关闭"), this);
    btns->addWidget(close);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    lay->addLayout(btns);

    connect(m_summary, &QTableWidget::itemSelectionChanged, this, [this] {
        const QList<qint64> sel = selectedStudents();
        const qint64 id = sel.size() == 1 ? sel.first() : 0;
        if (id != m_current) {
            m_current = id;
            reloadEntries();
        }
    });
    m_info->setText(student ? T("课时由老师和管理员记录。有疑问请联系老师。")
                    : admin ? T("剩余课时 %1 节及以下的标红，提醒续费。记错了可以作废，账目会留在操作日志里。可以按住 Ctrl 选几个学生一起记上课。").arg(kLowLessons)
                            : T("剩余课时 %1 节及以下的标红。可以按住 Ctrl 选几个学生一起记上课；只能作废自己记的上课。").arg(kLowLessons));
    // 不超过屏幕可用区域（任务栏不挡住底下的按钮）
    QSize sz(admin ? 900 : 760, 560);
    if (QScreen *scr = parent ? parent->screen() : screen()) sz = sz.boundedTo(scr->availableGeometry().size() * 0.85);
    resize(sz);
    // 打开后整个窗口挪进屏幕可用区域（含标题栏）
    QTimer::singleShot(0, this, [this] {
        QScreen *scr = screen();
        if (!scr) return;
        const QRect avail = scr->availableGeometry();
        QRect f = frameGeometry();
        f.moveLeft(qBound(avail.left(), f.left(), qMax(avail.left(), avail.right() - f.width())));
        f.moveTop(qBound(avail.top(), f.top(), qMax(avail.top(), avail.bottom() - f.height())));
        move(f.topLeft());
    });
    reloadSummary();
}

void TuitionDialog::reloadSummary()
{
    QPointer<TuitionDialog> self(this);
    api()->get(QStringLiteral("/tuition/summary"), [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, self->windowTitle(), r); return; }
        self->m_sumList = r.data.toArray();
        self->fillSummary();
        if (self->m_sumList.size() == 1) self->m_summary->selectRow(0);
    });
}

void TuitionDialog::fillSummary()
{
    const QList<qint64> sel = selectedStudents();
    const QSet<qint64> keep(sel.begin(), sel.end());
    const QString f = m_filter ? m_filter->text().trimmed() : QString();
    const bool money = m_summary->columnCount() == 7;
    m_summary->setRowCount(0);
    for (const QJsonValue &v : m_sumList) {
        const QJsonObject o = v.toObject();
        const QString name = o["name"].toString(), user = o["username"].toString(), classes = o["classes"].toString();
        if (!f.isEmpty() && !name.contains(f, Qt::CaseInsensitive) && !user.contains(f, Qt::CaseInsensitive) && !classes.contains(f, Qt::CaseInsensitive))
            continue;
        const int left = o["lessons_left"].toInt();
        QStringList cells{name, user, classes, QString::number(left)};
        if (money) cells << Tuition::money(num(o["paid_cents"]));
        cells << o["last_lesson"].toString() << o["last_payment"].toString();
        const int row = m_summary->rowCount();
        m_summary->insertRow(row);
        for (int c = 0; c < cells.size(); ++c) {
            auto *it = new QTableWidgetItem(cells[c]);
            it->setData(Qt::UserRole, num(o["student_id"]));
            if (left <= kLowLessons) it->setForeground(QColor(0xc0, 0x30, 0x30));
            if (o["status"].toString() == QLatin1String("disabled")) it->setForeground(Qt::gray);
            if (c == 3 && left <= 0) { QFont fnt = it->font(); fnt.setBold(true); it->setFont(fnt); }
            m_summary->setItem(row, c, it);
        }
        if (keep.contains(num(o["student_id"]))) m_summary->selectRow(row);
    }
}

QList<qint64> TuitionDialog::selectedStudents() const
{
    QList<qint64> ids;
    for (const QModelIndex &i : m_summary->selectionModel()->selectedRows())
        ids << m_summary->item(i.row(), 0)->data(Qt::UserRole).toLongLong();
    return ids;
}

void TuitionDialog::reloadEntries()
{
    m_entries->setRowCount(0);
    m_entryList = {};
    const int serial = ++m_entrySerial;
    if (!m_current) return;
    QPointer<TuitionDialog> self(this);
    const qint64 sid = m_current;
    const QString path = api()->isStudent() ? QStringLiteral("/tuition/entries") : QStringLiteral("/tuition/entries?student_id=%1").arg(sid);
    api()->get(path, [self, sid, serial](const ApiResult &r) {
        if (!self || self->m_current != sid || self->m_entrySerial != serial) return;
        if (!r.ok) { Online::showError(self, self->windowTitle(), r); return; }
        self->m_entries->setRowCount(0);
        self->m_entryList = r.data.toArray();
        const bool money = self->m_entries->columnCount() == 7;
        for (const QJsonValue &v : self->m_entryList) {
            const QJsonObject o = v.toObject();
            const int l = o["lessons"].toInt();
            QStringList cells{o["occurred_on"].toString(), Tuition::kindName(o["kind"].toString()),
                              l == 0 ? QString() : (l > 0 ? "+" + QString::number(l) : QString::number(l))};
            if (money) cells << (num(o["amount_cents"]) ? Tuition::money(num(o["amount_cents"])) : QString());
            cells << o["note"].toString() << o["creator_name"].toString() << (o["voided"].toBool() ? T("已作废") : QString());
            const int row = self->m_entries->rowCount();
            self->m_entries->insertRow(row);
            for (int c = 0; c < cells.size(); ++c) {
                auto *it = new QTableWidgetItem(cells[c]);
                it->setData(Qt::UserRole, num(o["id"]));
                if (o["voided"].toBool()) {
                    it->setForeground(Qt::gray);
                    QFont f = it->font();
                    f.setStrikeOut(c < cells.size() - 1);
                    it->setFont(f);
                }
                self->m_entries->setItem(row, c, it);
            }
        }
    });
}

void TuitionDialog::addEntry(const QString &kind)
{
    const QList<qint64> ids = selectedStudents();
    if (ids.isEmpty()) { QMessageBox::information(this, windowTitle(), T("请先在上面选择学生。")); return; }
    if (ids.size() > 1 && kind != QLatin1String("lesson")) { QMessageBox::information(this, windowTitle(), T("缴费、退费和调整一次只能记一个学生。")); return; }
    QStringList names;
    for (const QModelIndex &i : m_summary->selectionModel()->selectedRows()) names << m_summary->item(i.row(), 0)->text();

    QDialog dlg(this);
    dlg.setWindowTitle(Tuition::kindName(kind));
    auto *form = new QFormLayout(&dlg);
    auto *who = new QLabel(names.join(QStringLiteral("、")), &dlg);
    who->setWordWrap(true);
    form->addRow(T("学生："), who);
    auto *date = new QDateEdit(QDate::currentDate(), &dlg);
    date->setCalendarPopup(true);
    date->setDisplayFormat("yyyy-MM-dd");
    form->addRow(T("日期："), date);
    QDoubleSpinBox *amount = nullptr;
    if (kind == QLatin1String("payment") || kind == QLatin1String("refund")) {
        amount = new QDoubleSpinBox(&dlg);
        amount->setRange(0, 1000000);
        amount->setDecimals(2);
        amount->setPrefix(QStringLiteral("¥"));
        amount->setSingleStep(100);
        form->addRow(kind == QLatin1String("payment") ? T("收到金额：") : T("退还金额："), amount);
    }
    auto *lessons = new QSpinBox(&dlg);
    if (kind == QLatin1String("adjust")) {
        lessons->setRange(-500, 500);
        lessons->setValue(1);
        form->addRow(T("增减课时："), lessons);
        auto *tip = new QLabel(T("正数加课时，负数减课时。"), &dlg);
        tip->setStyleSheet("color: gray;");
        form->addRow(tip);
    } else {
        lessons->setRange(0, 500);
        lessons->setValue(kind == QLatin1String("lesson") ? 1 : 0);
        if (kind == QLatin1String("lesson")) lessons->setMinimum(1);
        form->addRow(kind == QLatin1String("payment") ? T("购买课时：") : kind == QLatin1String("refund") ? T("退掉课时：") : T("扣课时："), lessons);
    }
    lessons->setSuffix(T(" 节"));
    auto *note = new QLineEdit(&dlg);
    note->setMaxLength(200);
    note->setPlaceholderText(kind == QLatin1String("payment") ? T("例如：秋季班 20 节，微信转账") : T("可以不填"));
    form->addRow(T("备注："), note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(T("记账"));
    bb->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return;
    if (amount && amount->value() == 0 && lessons->value() == 0) {
        QMessageBox::warning(this, windowTitle(), T("金额和课时不能都是 0。"));
        return;
    }
    if (kind == QLatin1String("adjust") && lessons->value() == 0) return;

    QJsonArray idArr;
    for (qint64 id : ids) idArr.append(id);
    QJsonObject body{{"kind", kind}, {"student_ids", idArr}, {"lessons", lessons->value()},
                     {"occurred_on", date->date().toString("yyyy-MM-dd")}, {"note", note->text().trimmed()}};
    if (amount) body["amount_cents"] = qRound64(amount->value() * 100);
    QPointer<TuitionDialog> self(this);
    api()->post(QStringLiteral("/tuition/entries"), body, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, self->windowTitle(), r); return; }
        self->reloadSummary();
        self->reloadEntries();
    });
}

void TuitionDialog::voidEntry()
{
    const auto rows = m_entries->selectionModel()->selectedRows();
    if (rows.isEmpty()) { QMessageBox::information(this, windowTitle(), T("请先在下面的明细里选一笔。")); return; }
    const int row = rows.first().row();
    const qint64 id = m_entries->item(row, 0)->data(Qt::UserRole).toLongLong();
    const QString desc = m_entries->item(row, 0)->text() + "  " + m_entries->item(row, 1)->text() + "  " + m_entries->item(row, 2)->text();
    if (QMessageBox::question(this, windowTitle(), T("作废这一笔吗？\n%1\n\n作废后不再计入余额，但明细里还看得到。").arg(desc)) != QMessageBox::Yes) return;
    QPointer<TuitionDialog> self(this);
    api()->post(QStringLiteral("/tuition/entries/%1/void").arg(id), QJsonObject{}, [self](const ApiResult &r) {
        if (!self) return;
        if (!r.ok) { Online::showError(self, self->windowTitle(), r); return; }
        self->reloadSummary();
        self->reloadEntries();
    });
}

// 导出成 CSV（带 BOM，Excel 直接打开不乱码）
void TuitionDialog::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, T("导出表格"), T("课时统计-%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")), T("表格 (*.csv)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, windowTitle(), T("无法写入文件：%1").arg(path));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    auto esc = [](QString s) { s.replace('"', "\"\""); return '"' + s + '"'; };
    QStringList head;
    for (int c = 0; c < m_summary->columnCount(); ++c) head << esc(m_summary->horizontalHeaderItem(c)->text());
    out << head.join(',') << "\r\n";
    for (int r = 0; r < m_summary->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_summary->columnCount(); ++c) cells << esc(m_summary->item(r, c)->text());
        out << cells.join(',') << "\r\n";
    }
}
