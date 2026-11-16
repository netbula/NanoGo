#include "Rules.h"
#include "I18n.h"
#include <QComboBox>
#include <QJsonObject>

namespace Rules {

const QList<Info> &all()
{
    // 中国 2027 试行版：数子，黑贴 3.5 子（= 7 目），会有和棋；其余和 2002 版一样
    // 应氏（计点制）：填满计点，贴 8 点，允许自杀；KataGo 没有现成写法，用自定义规则近似
    static const QList<Info> list = {
        {"chinese", "中国规则（2002 版，贴 3¾ 子）", "Chinese", 7.5},
        {"chinese-2027", "中国规则（2027 试行版，贴 3.5 子，可和棋）", "Chinese-2027", 7.0},
        {"japanese", "日本规则", "Japanese", 6.5},
        {"korean", "韩国规则", "Korean", 6.5},
        {"ing", "应氏规则（计点制）", "GOE", 8.0},
        {"aga", "AGA 规则", "AGA", 7.5},
        {"new-zealand", "新西兰规则", "NZ", 7.0},
        {"tromp-taylor", "Tromp-Taylor", "Tromp-Taylor", 7.5},
    };
    return list;
}

const Info &get(const QString &id)
{
    for (const Info &i : all())
        if (i.id == id) return i;
    return all().first();
}

QString name(const QString &id) { return T(get(id).name); }

QJsonValue katago(const QString &id)
{
    if (id == QLatin1String("chinese-2027")) return QStringLiteral("chinese");
    if (id == QLatin1String("ing"))
        return QJsonObject{{"ko", "SITUATIONAL"}, {"scoring", "AREA"}, {"tax", "NONE"}, {"suicide", true},
                           {"hasButton", false}, {"whiteHandicapBonus", "0"}};
    return get(id).id;
}

QString fromSgf(const QString &ru)
{
    const QString r = ru.toLower();
    if (r.contains("2027")) return QStringLiteral("chinese-2027");
    if (r.contains("japan")) return QStringLiteral("japanese");
    if (r.contains("korea")) return QStringLiteral("korean");
    if (r.contains("goe") || r.contains("ing")) return QStringLiteral("ing");
    if (r.contains("aga")) return QStringLiteral("aga");
    if (r == QLatin1String("nz") || r.contains("zealand")) return QStringLiteral("new-zealand");
    if (r.contains("tromp") || r == QLatin1String("tt")) return QStringLiteral("tromp-taylor");
    return QStringLiteral("chinese");
}

void fillBox(QComboBox *box, const QString &current)
{
    for (const Info &i : all()) box->addItem(T(i.name), i.id);
    box->setCurrentIndex(qMax(0, box->findData(current)));
}

}
