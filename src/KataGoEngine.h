#pragma once
#include <QSet>
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
    // 失败原因是配置或模型问题（换引擎也没用），而不是显卡/引擎本身起不来
    bool failureIsConfigOrModel() const;
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
    void tuned(int threads);  // 自动测试硬件得出的最佳线程数
    // 第一次调优 / 测试硬件的进度（按 KataGo 的实际输出计算）；percent < 0 表示结束
    void prepareProgress(int percent, const QString &text);

private:
    void setState(State s, const QString &msg);
    void onStdout();
    void onStderr();
    void onFinished(int code, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError err);
    QString writeConfig(const AppSettings &s, bool forBenchmark = false) const;
    QString explainFailure() const;
    void launchAnalysis();
    void runBenchmark();
    void onBenchmarkOutput();
    void onBenchmarkFinished();
    QProcess *newProcess();

    QProcess *m_proc = nullptr;
    State m_state = State::Stopped;
    QByteArray m_outBuf, m_errBuf;
    QStringList m_recentErr;
    QStringList m_devices;
    QString m_deviceName;
    int m_nextId = 0;
    bool m_stopping = false;
    bool m_benchmarking = false;
    bool m_tuneFailed = false;
    void parseProgress(const QString &line);
    int m_tunePhases = 0;
    double m_tuneDoneWeight = 0, m_tuneCurWeight = 0;      // 显卡调优已经开始的项目数
    int m_benchTotal = 0, m_benchDone = 0;
    QSet<int> m_benchSeen;
    AppSettings m_settings;
    QStringList m_benchLines;
};
