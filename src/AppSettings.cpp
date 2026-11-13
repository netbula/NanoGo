#include "AppSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

QString AppSettings::appDir() { return QCoreApplication::applicationDirPath(); }

QString AppSettings::dataDir()
{
    QDir d(appDir());
    d.mkpath("data");
    return d.filePath("data");
}

QString AppSettings::iniPath() { return QDir(appDir()).filePath("nanogo.ini"); }

void AppSettings::load()
{
    QSettings s(iniPath(), QSettings::IniFormat);
    katagoPath = s.value("engine/katago", katagoPath).toString();
    modelPath = s.value("engine/model", modelPath).toString();
    gpuDevice = s.value("engine/gpuDevice", gpuDevice).toInt();
    searchThreads = s.value("engine/searchThreads", searchThreads).toInt();
    maxVisits = s.value("engine/maxVisits", maxVisits).toInt();
    komi = s.value("game/komi", komi).toDouble();
    rules = s.value("game/rules", rules).toString();
    if (katagoPath.isEmpty() || modelPath.isEmpty()) autoDetect();
}

void AppSettings::save() const
{
    QSettings s(iniPath(), QSettings::IniFormat);
    s.setValue("engine/katago", katagoPath);
    s.setValue("engine/model", modelPath);
    s.setValue("engine/gpuDevice", gpuDevice);
    s.setValue("engine/searchThreads", searchThreads);
    s.setValue("engine/maxVisits", maxVisits);
    s.setValue("game/komi", komi);
    s.setValue("game/rules", rules);
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
    // 只在 NanoGo 自己的文件夹里找 KataGo 引擎，不依赖 KaTrain
    const QStringList exeDirs = {QDir(appDir()).filePath("katago"), "C:/dev/NanoGo/katago"};
    if (katagoPath.isEmpty()) {
        for (const QString &d : exeDirs) {
            // 优先 OpenCL 版（显卡），不要 +bs50 那种特殊版本
            QString p = newestMatch(d, {"katago*opencl*.exe"});
            QDir dir(d);
            for (const QFileInfo &f : dir.entryInfoList({"katago*opencl*.exe"}, QDir::Files))
                if (!f.fileName().contains('+')) { p = f.absoluteFilePath(); break; }
            if (p.isEmpty()) p = newestMatch(d, {"katago*.exe"});
            if (!p.isEmpty()) { katagoPath = p; break; }
        }
    }
    if (modelPath.isEmpty()) {
        const QStringList modelDirs = {QDir(appDir()).filePath("models"), "C:/dev/NanoGo/models",
                                       home + "/Downloads"};
        for (const QString &d : modelDirs) {
            QString p = newestMatch(d, {"*.bin.gz"}, "zhizi");
            if (!p.isEmpty() && !QFileInfo(p).fileName().contains("human")) { modelPath = p; break; }
        }
    }
}
