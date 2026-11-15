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
    QString tunedFor;        // 测试结果对应的 引擎|模型，换了就重新测
    int maxVisits = 1000;    // 每个局面的最大计算量
    double komi = 7.5;
    QString rules = QStringLiteral("chinese");
    QString language = QStringLiteral("auto"); // auto / zh / en / ja / ko
    bool autoUpdateModel = true;  // 自动跟踪官方最强模型
    QDateTime lastModelCheck;

    static QString appDir();
    static QString rootDir();    // NanoGo 的根目录：exe 往上找含 katago 或 models 文件夹的目录，找不到就是 exe 目录
    static QString dataDir();    // 程序生成的配置、日志、调优文件放这里
    static QString iniPath();
    static QString modelsDir();  // 自动下载的模型放这里

    QString tuneKey() const { return katagoPath + "|" + modelPath; }
    bool needsTuning() const { return searchThreads <= 0 && (tunedThreads <= 0 || tunedFor != tuneKey()); }
    int effectiveThreads() const { return searchThreads > 0 ? searchThreads : (tunedThreads > 0 ? tunedThreads : 8); }
    QString effectiveLanguage() const;

    void load();
    void save() const;
    void autoDetect();       // 第一次运行时自动找 KataGo 和模型
};
