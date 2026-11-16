#pragma once
#include <QJsonValue>
#include <QList>
#include <QString>

class QComboBox;

// 围棋规则：界面名称、SGF 的 RU 写法、交给 KataGo 的写法、默认贴目
namespace Rules {
struct Info {
    QString id;          // 程序内部用，如 chinese、chinese-2027
    const char *name;    // 界面名称（T() 翻译）
    QString sgf;         // SGF 的 RU 值
    double komi;         // 默认贴目（目）
};
const QList<Info> &all();
const Info &get(const QString &id);       // 不认识的按中国规则
QString name(const QString &id);           // 当前语言的名称
QJsonValue katago(const QString &id);      // KataGo 分析请求里的 rules
QString fromSgf(const QString &ru);        // 读 SGF 的 RU
void fillBox(QComboBox *box, const QString &current);
}
