#include "KataGoEngine.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

KataGoEngine::KataGoEngine(QObject *parent) : QObject(parent) {}

KataGoEngine::~KataGoEngine() { stop(); }

void KataGoEngine::setState(State s, const QString &msg)
{
    m_state = s;
    emit stateChanged(s, msg);
}

QString KataGoEngine::writeConfig(const AppSettings &s) const
{
    const QString dir = AppSettings::dataDir();
    const QString path = QDir(dir).filePath("analysis.cfg");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return {};
    QTextStream o(&f);
    o << "# 由 NanoGo 自动生成，每次启动引擎都会重写\n";
    o << "logToStderr = true\n";
    o << "logAllRequests = false\n";
    o << "logAllResponses = false\n";
    o << "logSearchInfo = false\n";
    o << "reportAnalysisWinratesAs = BLACK\n";
    o << "homeDataDir = " << QDir::toNativeSeparators(dir) << "\n";
    o << "numAnalysisThreads = 2\n";
    o << "numSearchThreadsPerAnalysisThread = " << s.searchThreads << "\n";
    // 必填项：一次送进显卡的最大局面数。两个分析线程各自的搜索线程加起来正好填满一批。
    o << "nnMaxBatchSize = " << qMax(8, s.searchThreads * 2) << "\n";
    o << "nnCacheSizePowerOfTwo = 21\n";
    o << "nnMutexPoolSizePowerOfTwo = 17\n";
    o << "wideRootNoise = 0.04\n";
    // 你的电脑上 OpenCL 0 号是 AMD 集成显卡，1 号才是 5060 Ti。
    // -1 表示不写这一项，让 KataGo 自动挑最强的显卡（实测会选 5060 Ti）。
    if (s.gpuDevice >= 0) o << "openclDeviceToUseThread0 = " << s.gpuDevice << "\n";
    return path;
}

void KataGoEngine::start(const AppSettings &s)
{
    stop();
    m_recentErr.clear();
    m_devices.clear();
    m_deviceName.clear();

    if (s.katagoPath.isEmpty() || !QFileInfo::exists(s.katagoPath)) {
        setState(State::Failed, QStringLiteral("找不到 KataGo 程序，请在“设置”里选择 katago.exe。\n当前路径：%1").arg(s.katagoPath));
        return;
    }
    if (s.modelPath.isEmpty() || !QFileInfo::exists(s.modelPath)) {
        setState(State::Failed, QStringLiteral("找不到模型文件，请在“设置”里选择 .bin.gz 模型。\n当前路径：%1").arg(s.modelPath));
        return;
    }
    const QString cfg = writeConfig(s);
    if (cfg.isEmpty()) {
        setState(State::Failed, QStringLiteral("无法写入配置文件到 %1").arg(AppSettings::dataDir()));
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(AppSettings::dataDir());
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &KataGoEngine::onStdout);
    connect(m_proc, &QProcess::readyReadStandardError, this, &KataGoEngine::onStderr);
    connect(m_proc, &QProcess::finished, this, &KataGoEngine::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, &KataGoEngine::onError);

    m_stopping = false;
    setState(State::Starting, QStringLiteral("正在启动 KataGo（第一次使用新模型时显卡调优可能要几分钟）…"));
    m_proc->start(s.katagoPath, {"analysis", "-config", cfg, "-model", s.modelPath});
}

void KataGoEngine::stop()
{
    if (!m_proc) return;
    m_stopping = true;
    m_proc->disconnect(this);
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->closeWriteChannel(); // KataGo 读到 EOF 会自行退出
        if (!m_proc->waitForFinished(3000)) {
            m_proc->kill();
            m_proc->waitForFinished(2000);
        }
    }
    m_proc->deleteLater();
    m_proc = nullptr;
    if (m_state != State::Failed) setState(State::Stopped, QStringLiteral("引擎已停止"));
}

QString KataGoEngine::analyze(QJsonObject query)
{
    if (!m_proc || m_state != State::Ready) return {};
    const QString id = QStringLiteral("q%1").arg(++m_nextId);
    query["id"] = id;
    m_proc->write(QJsonDocument(query).toJson(QJsonDocument::Compact) + "\n");
    return id;
}

void KataGoEngine::terminate(const QString &id)
{
    if (!m_proc || m_state != State::Ready || id.isEmpty()) return;
    QJsonObject q{{"id", QStringLiteral("t%1").arg(++m_nextId)}, {"action", "terminate"}, {"terminateId", id}};
    m_proc->write(QJsonDocument(q).toJson(QJsonDocument::Compact) + "\n");
}

void KataGoEngine::terminateAll()
{
    if (!m_proc || m_state != State::Ready) return;
    QJsonObject q{{"id", QStringLiteral("t%1").arg(++m_nextId)}, {"action", "terminate_all"}};
    m_proc->write(QJsonDocument(q).toJson(QJsonDocument::Compact) + "\n");
}

void KataGoEngine::onStdout()
{
    m_outBuf += m_proc->readAllStandardOutput();
    int nl;
    while ((nl = m_outBuf.indexOf('\n')) >= 0) {
        QByteArray line = m_outBuf.left(nl).trimmed();
        m_outBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit logLine(QString::fromUtf8(line));
            continue;
        }
        QJsonObject obj = doc.object();
        if (obj.contains("error")) {
            emit logLine(QStringLiteral("引擎返回错误：") + obj.value("error").toString());
            continue;
        }
        if (obj.contains("warning")) {
            emit logLine(QStringLiteral("引擎警告：") + obj.value("warning").toString());
            continue;
        }
        if (obj.contains("moveInfos") || obj.contains("rootInfo")) emit analysisReceived(obj);
    }
}

void KataGoEngine::onStderr()
{
    m_errBuf += m_proc->readAllStandardError();
    int nl;
    static const QRegularExpression foundRe(QStringLiteral("Found OpenCL Device (\\d+): (.*)"));
    static const QRegularExpression usingRe(QStringLiteral("Using OpenCL Device (\\d+): (.*?)(\\(|$)"));
    while ((nl = m_errBuf.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(m_errBuf.left(nl)).trimmed();
        m_errBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        emit logLine(line);
        m_recentErr.append(line);
        if (m_recentErr.size() > 40) m_recentErr.removeFirst();

        auto m = foundRe.match(line);
        if (m.hasMatch()) m_devices.append(QStringLiteral("%1: %2").arg(m.captured(1), m.captured(2).trimmed()));
        auto u = usingRe.match(line);
        if (u.hasMatch()) m_deviceName = QStringLiteral("%1 号显卡 %2").arg(u.captured(1), u.captured(2).trimmed());
        if (line.contains("Started, ready to begin handling requests")) {
            setState(State::Ready, m_deviceName.isEmpty() ? QStringLiteral("引擎已就绪")
                                                          : QStringLiteral("引擎已就绪，使用 %1").arg(m_deviceName));
        } else if (m_state == State::Starting && line.contains("Performing autotuning")) {
            setState(State::Starting, QStringLiteral("第一次在这块显卡上运行该模型，正在调优，大约需要几分钟，请稍候…"));
        }
    }
}

QString KataGoEngine::explainFailure() const
{
    const QString all = m_recentErr.join('\n');
    QString hint;
    static const QRegularExpression missingKey(QStringLiteral("Could not find key '([^']+)'"));
    const auto mk = missingKey.match(all);
    if (mk.hasMatch())
        hint = QStringLiteral("配置文件缺少必填项 %1。这是 NanoGo 的问题，请把这个提示发给开发者。").arg(mk.captured(1));
    else if (all.contains("config", Qt::CaseInsensitive) && all.contains("exception", Qt::CaseInsensitive))
        hint = QStringLiteral("配置文件有问题。");
    else if (all.contains("CUDA", Qt::CaseInsensitive) && all.contains("error", Qt::CaseInsensitive))
        hint = QStringLiteral("CUDA 版 KataGo 与显卡或驱动不匹配（RTX 50 系列需要 CUDA 12.8 以上编译的版本）。建议在设置里改用 OpenCL 版。");
    else if (all.contains("OpenCL", Qt::CaseInsensitive) && (all.contains("error", Qt::CaseInsensitive) || all.contains("CL_", Qt::CaseSensitive)))
        hint = QStringLiteral("显卡（OpenCL）初始化失败。请在设置里把“显卡”改成“自动”，或换一个编号（你电脑上 NVIDIA 显卡通常是 1 号）。");
    else if (all.contains("exception", Qt::CaseInsensitive) && all.contains("model", Qt::CaseInsensitive)
             && (all.contains("open", Qt::CaseInsensitive) || all.contains("load", Qt::CaseInsensitive) || all.contains("gz", Qt::CaseInsensitive)))
        hint = QStringLiteral("模型文件读取失败，可能文件损坏或下载不完整。");
    else
        hint = QStringLiteral("KataGo 意外退出。");
    const QStringList tail = m_recentErr.mid(qMax(0, m_recentErr.size() - 6));
    return hint + QStringLiteral("\n\n最后几行输出：\n") + tail.join('\n');
}

void KataGoEngine::onFinished(int code, QProcess::ExitStatus)
{
    if (m_stopping) return;
    setState(State::Failed, QStringLiteral("KataGo 已退出（代码 %1）。").arg(code) + explainFailure());
}

void KataGoEngine::onError(QProcess::ProcessError err)
{
    if (m_stopping) return;
    if (err == QProcess::FailedToStart)
        setState(State::Failed, QStringLiteral("KataGo 无法启动：%1\n可能缺少运行库（同目录下的 dll），或者被杀毒软件拦截。").arg(m_proc->errorString()));
}
