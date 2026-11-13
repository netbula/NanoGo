#pragma once
#include "AppSettings.h"
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QStringList>

// 通过 KataGo 的 analysis 协议（每行一个 JSON）和引擎通信
class KataGoEngine : public QObject {
    Q_OBJECT
public:
    enum class State { Stopped, Starting, Ready, Failed };
    Q_ENUM(State)

    explicit KataGoEngine(QObject *parent = nullptr);
    ~KataGoEngine() override;

    void start(const AppSettings &settings);
    void stop();
    State state() const { return m_state; }
    QString deviceName() const { return m_deviceName; }
    QStringList openclDevices() const { return m_devices; }

    // 发送分析请求，返回请求 id
    QString analyze(QJsonObject query);
    void terminate(const QString &id);
    void terminateAll();

signals:
    void stateChanged(KataGoEngine::State state, const QString &message);
    void analysisReceived(const QJsonObject &response);
    void logLine(const QString &line);

private:
    void setState(State s, const QString &msg);
    void onStdout();
    void onStderr();
    void onFinished(int code, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError err);
    QString writeConfig(const AppSettings &s) const;
    QString explainFailure() const;

    QProcess *m_proc = nullptr;
    State m_state = State::Stopped;
    QByteArray m_outBuf, m_errBuf;
    QStringList m_recentErr;
    QStringList m_devices;
    QString m_deviceName;
    int m_nextId = 0;
    bool m_stopping = false;
};
