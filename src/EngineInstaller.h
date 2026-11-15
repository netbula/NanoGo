#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QCryptographicHash;

// 第一次运行时从 KataGo 官方 GitHub 发布页下载引擎（安装包里不带 KataGo）。
// 版本和 SHA-256 固定写在程序里，下载后校验，再解压到 <程序目录>/katago/<子目录>。
struct EngineBuild {
    QString kind;     // opencl / eigenavx2 / eigen
    QString subdir;   // katago 下的子目录
    QString url;
    QString sha256;
    qint64 bytes = 0;
    bool isNull() const { return kind.isEmpty(); }
};

class EngineInstaller : public QObject {
    Q_OBJECT
public:
    explicit EngineInstaller(QObject *parent = nullptr);

    static QString version();
    static EngineBuild build(const QString &kind);
    // 按本机硬件挑选：有可用显卡用 OpenCL 版，否则按 CPU 是否支持 AVX2 选 CPU 版
    static EngineBuild bestForThisPc(QString *reason = nullptr);
    static EngineBuild cpuBuildForThisPc();

    void install(const EngineBuild &b);
    bool busy() const { return m_reply != nullptr; }

signals:
    void progress(qint64 done, qint64 total);
    void installed(const QString &katagoExe);
    void failed(const QString &message);

private:
    void finishDownload();
    QString extract(const QString &zip, const QString &dir);

    QNetworkAccessManager *m_net;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    QCryptographicHash *m_hash = nullptr;
    EngineBuild m_target;
};
