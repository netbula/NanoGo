#include "AppSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <algorithm>

QString AppSettings::appDir() { return QCoreApplication::applicationDirPath(); }

QString AppSettings::dataDir()
{
    QDir d(appDir());
    d.mkpath("data");
    return d.filePath("data");
}

QString AppSettings::rootDir()
{
    QDir d(appDir());
    for (int i = 0; i < 4; ++i) {
        if (d.exists("katago") || d.exists("models")) return d.absolutePath();
        if (!d.cdUp()) break;
    }
    return appDir();
}

QString AppSettings::modelsDir()
{
    QDir d(rootDir());
    d.mkpath("models");
    return d.filePath("models");
}

QString AppSettings::humanModelPath()
{
    return QDir(modelsDir()).filePath(QStringLiteral(HUMAN_MODEL_FILE));
}

bool AppSettings::isHumanModel(const QString &fileName)
{
    return fileName.contains(QStringLiteral("human"), Qt::CaseInsensitive);
}

QString AppSettings::effectiveLanguage() const
{
    if (language != "auto") return language;
    const QString sys = QLocale::system().name(); // 例如 zh_CN, ja_JP
    if (sys.startsWith("zh")) return "zh";
    if (sys.startsWith("ja")) return "ja";
    if (sys.startsWith("ko")) return "ko";
    return "en";
}

QString AppSettings::iniPath() { return QDir(appDir()).filePath("nanogo.ini"); }

void AppSettings::load()
{
    QSettings s(iniPath(), QSettings::IniFormat);
    katagoPath = s.value("engine/katago", katagoPath).toString();
    modelPath = s.value("engine/model", modelPath).toString();
    gpuDevice = s.value("engine/gpuDevice", gpuDevice).toInt();
    searchThreads = s.value("engine/searchThreadsManual", searchThreads).toInt(); // 旧版的 searchThreads=16 不再沿用，改为自动
    tunedThreads = s.value("engine/tunedThreads", tunedThreads).toInt();
    tunedFor = s.value("engine/tunedFor", tunedFor).toString();
    language = s.value("ui/language", language).toString();
    maxVisits = s.value("engine/maxVisits", maxVisits).toInt();
    komi = s.value("game/komi", komi).toDouble();
    rules = s.value("game/rules", rules).toString();
    autoUpdateModel = s.value("model/autoUpdate", autoUpdateModel).toBool();
    lastModelCheck = s.value("model/lastCheck").toDateTime();
    if (katagoPath.isEmpty() || modelPath.isEmpty() || !QFileInfo::exists(katagoPath) || !QFileInfo::exists(modelPath)) {
        autoDetect();
        save(); // 把自动找到的路径写进 nanogo.ini，方便查看和修改
    }
}

void AppSettings::save() const
{
    QSettings s(iniPath(), QSettings::IniFormat);
    s.setValue("engine/katago", katagoPath);
    s.setValue("engine/model", modelPath);
    s.setValue("engine/gpuDevice", gpuDevice);
    s.setValue("engine/searchThreadsManual", searchThreads);
    s.setValue("engine/tunedThreads", tunedThreads);
    s.setValue("engine/tunedFor", tunedFor);
    s.setValue("ui/language", language);
    s.setValue("engine/maxVisits", maxVisits);
    s.setValue("game/komi", komi);
    s.setValue("game/rules", rules);
    s.setValue("model/autoUpdate", autoUpdateModel);
    s.setValue("model/lastCheck", lastModelCheck);
}

static QString newestMatch(const QString &dir, const QStringList &patterns, const QString &prefer = {})
{
    QDir d(dir);
    if (!d.exists()) return {};
    const QFileInfoList files = d.entryInfoList(patterns, QDir::Files, QDir::Size); // 大的优先
    for (const QFileInfo &f : files)
        if (!prefer.isEmpty() && f.fileName().contains(prefer, Qt::CaseInsensitive)) return f.absoluteFilePath();
    for (const QFileInfo &f : files)
        if (!AppSettings::isHumanModel(f.fileName())) return f.absoluteFilePath(); // 人类风格模型不能当主模型
    return {};
}

QStringList AppSettings::findKatagoBuilds()
{
    QStringList dirs;
    for (const QString &base : {QDir(appDir()).filePath("katago"), QDir(rootDir()).filePath("katago")}) {
        if (!QDir(base).exists() || dirs.contains(QDir(base).absolutePath())) continue;
        dirs << QDir(base).absolutePath();
        for (const QFileInfo &sub : QDir(base).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) dirs << sub.absoluteFilePath();
    }
    QStringList exes;
    for (const QString &d : dirs)
        for (const QFileInfo &f : QDir(d).entryInfoList({"katago*.exe", "katago"}, QDir::Files))
            if (!f.fileName().contains('+')) exes << f.absoluteFilePath(); // 带 + 的是特殊编译版本，跳过
    // 显卡版优先，其次 CPU 版。路径里的文件夹名（如 opencl / cpu）也参与判断
    auto rank = [](const QString &path) {
        const QString p = path.toLower();
        if (p.contains("opencl")) return 0;
        if (p.contains("trt") || p.contains("tensorrt")) return 1;
        if (p.contains("cuda")) return 2;
        if (p.contains("eigenavx2") || p.contains("avx2")) return 4;
        if (p.contains("eigen") || p.contains("cpu")) return 5;
        return 3;
    };
    std::stable_sort(exes.begin(), exes.end(), [&](const QString &x, const QString &y) { return rank(x) < rank(y); });
    return exes;
}

QString AppSettings::nextKatagoBuild() const
{
    const QStringList builds = findKatagoBuilds();
    const int i = builds.indexOf(QFileInfo(katagoPath).absoluteFilePath());
    if (i >= 0 && i + 1 < builds.size()) return builds[i + 1];
    return {};
}

void AppSettings::autoDetect()
{
    const QString home = QDir::homePath();
    if (katagoPath.isEmpty() || !QFileInfo::exists(katagoPath)) {
        const QStringList builds = findKatagoBuilds();
        katagoPath = builds.isEmpty() ? QString() : builds.first();
    }
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        modelPath.clear();
        // 只在程序自己的 models 文件夹里找；找不到时由首次运行自动下载最强模型
        const QStringList modelDirs = {modelsDir(), QDir(appDir()).filePath("models")};
        for (const QString &d : modelDirs) {
            QString p = newestMatch(d, {"*.bin.gz"});
            if (!p.isEmpty()) { modelPath = p; break; }
        }
    }
}
