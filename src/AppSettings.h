#pragma once
#include <QString>

// 程序设置，保存在 exe 同目录的 nanogo.ini
struct AppSettings {
    QString katagoPath;      // KataGo 程序（推荐 OpenCL 版）
    QString modelPath;       // 神经网络模型 .bin.gz
    int gpuDevice = -1;      // OpenCL 显卡编号，-1 = 自动选择独立显卡
    int searchThreads = 16;  // 5060 Ti 实测 16 线程最合适
    int maxVisits = 1000;    // 每个局面的最大计算量
    double komi = 7.5;
    QString rules = QStringLiteral("chinese");

    static QString appDir();
    static QString dataDir();    // 程序生成的配置、日志、调优文件放这里
    static QString iniPath();

    void load();
    void save() const;
    void autoDetect();       // 第一次运行时自动找 KataGo 和模型
};
