#include "I18n.h"
#include "ModelUpdater.h"
#include "AppSettings.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <cmath>

static const char *kApiUrl = "https://katagotraining.org/api/networks/";

ModelUpdater::ModelUpdater(QObject *parent) : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

void ModelUpdater::checkForUpdates()
{
    if (m_reply) return;
    m_list.clear();
    fetchPage(QString::fromLatin1(kApiUrl));
}

void ModelUpdater::fetchPage(const QString &url)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "NanoGo/0.1");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(30000);
    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *r = m_reply;
        m_reply = nullptr;
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            emit failed(T("无法连接 KataGo 官方训练站：%1").arg(r->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        QJsonArray results;
        QString next;
        if (doc.isArray()) results = doc.array();
        else {
            results = doc.object().value("results").toArray();
            next = doc.object().value("next").toString();
        }
        const double k = 400.0 / std::log(10.0);
        for (const auto &v : results) {
            const QJsonObject o = v.toObject();
            NetworkInfo n;
            n.name = o.value("name").toString();
            n.url = o.value("model_file").toString();
            n.sha256 = o.value("model_file_sha256").toString();
            n.bytes = qint64(o.value("model_file_bytes").toDouble());
            n.elo = o.value("log_gamma").toDouble() * k;
            n.eloUncertainty = o.value("log_gamma_uncertainty").toDouble() * k;
            n.eloLower = o.value("log_gamma_lower_confidence").toDouble() * k;
            n.games = o.value("log_gamma_game_count").toInt();
            n.created = o.value("created_at").toString();
            if (!n.name.isEmpty() && !n.url.isEmpty()) m_list.append(n);
        }
        if (!next.isEmpty() && m_list.size() < 20000) fetchPage(next);
        else emit listReady(m_list);
    });
}

NetworkInfo ModelUpdater::strongest(const QList<NetworkInfo> &list, int minGames)
{
    NetworkInfo best;
    bool found = false;
    for (const NetworkInfo &n : list) {
        if (n.games < minGames) continue;
        if (!found || n.eloLower > best.eloLower) { best = n; found = true; }
    }
    return best;
}

const NetworkInfo *ModelUpdater::find(const QList<NetworkInfo> &list, const QString &modelPath)
{
    QString base = QFileInfo(modelPath).fileName();
    base.remove(".bin.gz").remove(".txt.gz");
    for (const NetworkInfo &n : list)
        if (n.name == base) return &n;
    return nullptr;
}

void ModelUpdater::download(const NetworkInfo &n)
{
    if (m_reply) return;
    m_target = n;
    const QString finalPath = QDir(AppSettings::modelsDir()).filePath(QFileInfo(QUrl(n.url).path()).fileName());
    if (QFileInfo::exists(finalPath) && (n.bytes <= 0 || QFileInfo(finalPath).size() == n.bytes)) {
        emit downloadFinished(finalPath);
        return;
    }
    m_file = new QFile(finalPath + ".part", this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(T("无法写入 %1").arg(m_file->fileName()));
        delete m_file; m_file = nullptr;
        return;
    }
    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);
    QNetworkRequest req{QUrl(n.url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "NanoGo/0.1");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &ModelUpdater::onDownloadData);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 d, qint64 t) {
        emit downloadProgress(d, t > 0 ? t : m_target.bytes);
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, finalPath] {
        onDownloadData();
        QNetworkReply *r = m_reply;
        m_reply = nullptr;
        r->deleteLater();
        m_file->close();
        const QString hash = QString::fromLatin1(m_hash->result().toHex());
        delete m_hash; m_hash = nullptr;
        const QString part = m_file->fileName();
        delete m_file; m_file = nullptr;
        if (r->error() != QNetworkReply::NoError) {
            QFile::remove(part);
            emit failed(T("模型下载失败：%1").arg(r->errorString()));
            return;
        }
        if (!m_target.sha256.isEmpty() && hash.compare(m_target.sha256, Qt::CaseInsensitive) != 0) {
            QFile::remove(part);
            emit failed(T("下载的模型校验不通过（文件不完整或被修改），已删除，请稍后重试。"));
            return;
        }
        QFile::remove(finalPath);
        if (!QFile::rename(part, finalPath)) {
            emit failed(T("无法保存模型到 %1").arg(finalPath));
            return;
        }
        emit downloadFinished(finalPath);
    });
}

void ModelUpdater::onDownloadData()
{
    if (!m_reply || !m_file) return;
    const QByteArray chunk = m_reply->readAll();
    m_hash->addData(chunk);
    m_file->write(chunk);
}

void ModelUpdater::cancel()
{
    if (m_reply) m_reply->abort();
}
