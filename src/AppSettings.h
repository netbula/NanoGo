#pragma once
#include <QDateTime>
#include <QString>

// 程序设置，保存在 exe 同目录的 nanogo.ini
struct AppSettings {
    QString katagoPath;      // KataGo 程序（推荐 OpenCL 版，也支持 CUDA / TensorRT / CPU 版）
    QString modelPath;       // 神经网络模型 .bin.gz
    int gpuDevice = -1;      // OpenCL 显卡编号，-1 = 让 KataGo 自动挑选
    int searchThreads = 0;   // 搜索线程数，0 = 自动（首次运行时测试本机硬件得出）
    int tunedThreads = 0;    // 自动测试的结果
    QString tunedFor;        // 测试结果对应的 引擎|模型|显卡，换了就重新测
    QString gpuName;         // 本次运行自动选中的显卡（不保存）
    int maxVisits = 1000;    // 每个局面的最大计算量
    double komi = 7.5;
    QString rules = QStringLiteral("chinese");
    QString language = QStringLiteral("auto"); // auto / zh / en / ja / ko
    bool autoUpdateModel = true;  // 自动跟踪官方最强模型
    QDateTime lastModelCheck;
    QString serverUrl;            // 教学服务端地址，空 = 只用单机版
    QString serverUser;           // 上次登录的用户名

    static QString appDir();
    static QString rootDir();    // NanoGo 的根目录：exe 往上找含 katago 或 models 文件夹的目录，找不到就是 exe 目录
    static QString dataDir();    // 程序生成的配置、日志、调优文件放这里
    static QString iniPath();
    static QString modelsDir();  // 自动下载的模型放这里
    // 模仿人类各段位下法的 KataGo 官方模型（按段位调强度用），第一次用时下载
#define HUMAN_MODEL_FILE "b18c384nbt-humanv0.bin.gz"
    static constexpr const char *humanModelUrl = "https://github.com/lightvector/KataGo/releases/download/v1.15.0/" HUMAN_MODEL_FILE;
    static constexpr const char *humanModelSha256 = "637746e44f0efe00ad1245a50aa9bbf0716efe364c43965ead97bd6835d84ab5";
    static constexpr qint64 humanModelBytes = 99066230;
    static QString humanModelPath();
    // 这个 KataGo 用什么计算：opencl / cuda / tensorrt / eigen（CPU），看不出来返回空。
    // 运行一次 `katago version` 得出（结果按路径缓存），不靠文件夹名字猜
    static QString katagoBackend(const QString &katagoPath);
    static bool isGpuBuild(const QString &katagoPath);
    static bool isCpuBuild(const QString &katagoPath);
    static bool isHumanModel(const QString &fileName);

    QString tuneKey() const { return katagoPath + "|" + modelPath + "|" + gpuName; }
    bool needsTuning() const { return searchThreads <= 0 && (tunedThreads <= 0 || tunedFor != tuneKey()); }
    int effectiveThreads() const { return searchThreads > 0 ? searchThreads : (tunedThreads > 0 ? tunedThreads : 8); }
    QString effectiveLanguage() const;

    void load();
    void save() const;
    void autoDetect();       // 第一次运行时自动找 KataGo 和模型
    // 找出所有可用的 KataGo，按 显卡版 → CPU 版 排好序（katago 文件夹及其子文件夹）
    static QStringList findKatagoBuilds();
    // 当前引擎起不来时的下一个候选（例如没有显卡时从 OpenCL 版换成 CPU 版），没有则返回空
    QString nextKatagoBuild() const;
};
