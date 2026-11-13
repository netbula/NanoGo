#pragma once
#include <QObject>
#include <QList>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QCryptographicHash;

// 从 KataGo 官方训练站 katagotraining.org 读取模型列表，挑出最强模型并下载
struct NetworkInfo {
    QString name;
    QString url;
    QString sha256;
    qint64 bytes = 0;
    double elo = 0;          // log_gamma × 400 / ln10
    double eloUncertainty = 0;
    double eloLower = 0;     // 可信下限，用来排“最强”
    int games = 0;
    QString created;
};

class ModelUpdater : public QObject {
    Q_OBJECT
public:
    explicit ModelUpdater(QObject *parent = nullptr);

    void checkForUpdates();               // 读列表 -> listReady
    void download(const NetworkInfo &n);  // 下载 -> downloadFinished
    void cancel();
    bool busy() const { return m_reply != nullptr; }

    // 在列表里找最强（对局数足够、可信下限最高）以及某个文件名对应的模型
    static NetworkInfo strongest(const QList<NetworkInfo> &list, int minGames = 1000);
    static const NetworkInfo *find(const QList<NetworkInfo> &list, const QString &modelPath);

signals:
    void listReady(const QList<NetworkInfo> &list);
    void downloadProgress(qint64 done, qint64 total);
    void downloadFinished(const QString &path);
    void failed(const QString &message);

private:
    void fetchPage(const QString &url);
    void onDownloadData();

    QNetworkAccessManager *m_net;
    QNetworkReply *m_reply = nullptr;
    QList<NetworkInfo> m_list;
    QFile *m_file = nullptr;
    QCryptographicHash *m_hash = nullptr;
    NetworkInfo m_target;
};
