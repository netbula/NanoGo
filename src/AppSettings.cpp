#include "AppSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>

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
    if (katagoPath.isEmpty() || modelPath.isEmpty()) {
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
    return files.isEmpty() ? QString() : files.first().absoluteFilePath();
}

void AppSettings::autoDetect()
{
    const QString home = QDir::homePath();
    const QString root = rootDir();
    if (katagoPath.isEmpty()) {
        // 只在 NanoGo 自己的 katago 文件夹里找引擎，不依赖其他程序
        const QStringList exeDirs = {QDir(appDir()).filePath("katago"), QDir(root).filePath("katago")};
        // 显卡版优先，其次 CPU 版；带 + 的是特殊编译版本，跳过
        const QStringList prefs = {"opencl", "trt", "tensorrt", "cuda", "eigenavx2", "eigen", ""};
        for (const QString &pref : prefs) {
            for (const QString &d : exeDirs) {
                for (const QFileInfo &f : QDir(d).entryInfoList({"katago*.exe", "katago"}, QDir::Files)) {
                    if (f.fileName().contains('+')) continue;
                    if (pref.isEmpty() || f.fileName().contains(pref, Qt::CaseInsensitive)) { katagoPath = f.absoluteFilePath(); break; }
                }
                if (!katagoPath.isEmpty()) break;
            }
            if (!katagoPath.isEmpty()) break;
        }
    }
    if (modelPath.isEmpty()) {
        const QStringList modelDirs = {modelsDir(), QDir(appDir()).filePath("models"), home + "/Downloads"};
        for (const QString &d : modelDirs) {
            QString p = newestMatch(d, {"*.bin.gz"});
            if (!p.isEmpty() && !QFileInfo(p).fileName().contains("human")) { modelPath = p; break; }
        }
    }
}
