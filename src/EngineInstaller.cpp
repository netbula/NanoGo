#include "EngineInstaller.h"
#include "AppSettings.h"
#include "HardwareInfo.h"
#include "I18n.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>

static const char *kVersion = "v1.18.1";

EngineInstaller::EngineInstaller(QObject *parent) : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

QString EngineInstaller::version() { return QString::fromLatin1(kVersion); }

EngineBuild EngineInstaller::build(const QString &kind)
{
    struct Row { const char *kind, *subdir, *sha; qint64 bytes; };
    static const Row rows[] = {
        {"opencl", "opencl", "1710db1903ab921aa6837a9599c8474f8a59f057650217c5d9bc125ee393a9ff", 6004137},
        {"eigenavx2", "cpu-avx2", "0d62ffa41ee04dd89dd1b80fe45e306c837231cb1e2dccb4f5780d0ca7c313db", 5899607},
        {"eigen", "cpu", "074485cf150c38aa3bb14ac9f54f2952ffefbceb44673709bbb8a83650bf95d6", 5903072},
    };
    for (const Row &r : rows) {
        if (kind != QLatin1String(r.kind)) continue;
        EngineBuild b;
        b.kind = r.kind;
        b.subdir = r.subdir;
        b.sha256 = r.sha;
        b.bytes = r.bytes;
        b.url = QStringLiteral("https://github.com/lightvector/KataGo/releases/download/%1/katago-%1-%2-windows-x64.zip")
                    .arg(version(), b.kind);
        return b;
    }
    return {};
}

EngineBuild EngineInstaller::cpuBuildForThisPc()
{
    return build(HardwareInfo::cpuHasAvx2() ? "eigenavx2" : "eigen");
}

EngineBuild EngineInstaller::bestForThisPc(QString *reason)
{
    QString why;
    const QList<GpuInfo> gpus = HardwareInfo::openclGpus(&why);
    if (!gpus.isEmpty()) {
        if (reason) *reason = T("检测到显卡 %1，下载显卡版（OpenCL）KataGo").arg(gpus[HardwareInfo::bestGpu(gpus)].name);
        return build("opencl");
    }
    if (reason) *reason = why + "\n" + T("没有可用的显卡，下载 CPU 版 KataGo");
    return cpuBuildForThisPc();
}

void EngineInstaller::install(const EngineBuild &b)
{
    if (m_reply || b.isNull()) return;
    m_target = b;
    const QString dir = QDir(AppSettings::rootDir()).filePath("katago/" + b.subdir);
    QDir().mkpath(dir);
    m_file = new QFile(QDir(dir).filePath("download.zip.part"), this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(T("无法写入 %1").arg(m_file->fileName()));
        delete m_file; m_file = nullptr;
        return;
    }
    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);
    QNetworkRequest req{QUrl(b.url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "NanoGo/0.1");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        const QByteArray chunk = m_reply->readAll();
        m_hash->addData(chunk);
        m_file->write(chunk);
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 d, qint64 t) {
        emit progress(d, t > 0 ? t : m_target.bytes);
    });
    connect(m_reply, &QNetworkReply::finished, this, &EngineInstaller::finishDownload);
}

void EngineInstaller::finishDownload()
{
    QNetworkReply *r = m_reply;
    const QByteArray rest = r->readAll();
    m_hash->addData(rest);
    m_file->write(rest);
    m_reply = nullptr;
    r->deleteLater();
    m_file->close();
    const QString hash = QString::fromLatin1(m_hash->result().toHex());
    delete m_hash; m_hash = nullptr;
    const QString part = m_file->fileName();
    delete m_file; m_file = nullptr;
    if (r->error() != QNetworkReply::NoError) {
        QFile::remove(part);
        emit failed(T("KataGo 引擎下载失败：%1").arg(r->errorString()));
        return;
    }
    if (hash.compare(m_target.sha256, Qt::CaseInsensitive) != 0) {
        QFile::remove(part);
        emit failed(T("下载的 KataGo 引擎校验不通过（文件不完整或被修改），已删除，请稍后重试。"));
        return;
    }
    const QString dir = QFileInfo(part).absolutePath();
    const QString zip = QDir(dir).filePath("download.zip");
    QFile::remove(zip);
    QFile::rename(part, zip);
    const QString err = extract(zip, dir);
    QFile::remove(zip);
    if (!err.isEmpty()) { emit failed(err); return; }
    QString exe;
    QDirIterator it(dir, {"katago.exe", "katago"}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) exe = it.next();
    if (exe.isEmpty()) { emit failed(T("解压后没有找到 katago.exe：%1").arg(dir)); return; }
    emit installed(QFileInfo(exe).absoluteFilePath());
}

// Windows 10/11 自带 tar.exe 可以解压 zip；没有的话用 PowerShell
QString EngineInstaller::extract(const QString &zip, const QString &dir)
{
    QString log;
    auto run = [&](const QString &prog, const QStringList &args) {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(prog, args);
        if (!p.waitForStarted(5000)) return false;
        p.waitForFinished(120000);
        log += QString::fromLocal8Bit(p.readAll());
        return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    };
#ifdef Q_OS_WIN
    const QString tar = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/tar.exe");
    if (QFileInfo::exists(tar) && run(tar, {"-xf", QDir::toNativeSeparators(zip), "-C", QDir::toNativeSeparators(dir)}))
        return {};
    if (run("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                           QStringLiteral("Expand-Archive -Force -LiteralPath '%1' -DestinationPath '%2'")
                               .arg(QDir::toNativeSeparators(zip), QDir::toNativeSeparators(dir))}))
        return {};
#else
    if (run("unzip", {"-o", zip, "-d", dir})) return {};
#endif
    return T("KataGo 引擎解压失败：") + log.trimmed().right(500);
}
