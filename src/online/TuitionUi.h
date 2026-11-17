#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// 学费和课时：管理员记缴费、退费、调整，看全部学生；老师给自己班的学生记上课；学生看自己的课时和明细
class TuitionDialog : public QDialog {
    Q_OBJECT
public:
    explicit TuitionDialog(QWidget *parent = nullptr);

private:
    void reloadSummary();
    void reloadEntries();
    void fillSummary();
    QList<qint64> selectedStudents() const;
    void addEntry(const QString &kind);
    void voidEntry();
    void exportCsv();

    QLineEdit *m_filter = nullptr;
    QTableWidget *m_summary, *m_entries;
    QLabel *m_info;
    QPushButton *m_void = nullptr;
    QJsonArray m_sumList, m_entryList;
    qint64 m_current = 0;
    int m_entrySerial = 0;  // 只用最后一次请求的结果，防止明细重复
};

namespace Tuition {
QString money(qint64 cents);  // ¥1,234.50
QString kindName(const QString &kind);
// 关对局室时给到场的学生记上课；roomId 防止重复记
void recordLessons(QWidget *parent, const QList<qint64> &students, qint64 roomId, const QString &note);
}
