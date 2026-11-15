#pragma once
#include <QList>
#include <QString>

// 自己识别显卡，弥补 KataGo 选卡能力弱的问题：
// KataGo 给集成显卡和独立显卡的打分可能只差一点点，有时会选成集成显卡。
struct GpuInfo {
    int clIndex = -1;        // KataGo 用的 OpenCL 设备编号（按平台顺序、全部设备依次编号）
    QString name;
    QString vendor;
    quint64 memoryMB = 0;    // OpenCL 报告的显存
    bool integrated = false; // 和系统共享内存 = 集成显卡
    int computeUnits = 0;
};

struct DisplayAdapter {      // Windows DXGI 看到的显卡（用于显示和诊断，能读到真实显存）
    QString name;
    quint32 vendorId = 0;
    quint64 dedicatedMB = 0;
    bool software = false;
};

namespace HardwareInfo {
QList<GpuInfo> openclGpus(QString *error = nullptr); // 没装 OpenCL 驱动时返回空，并给出原因
QList<DisplayAdapter> displayAdapters();
int bestGpu(const QList<GpuInfo> &gpus);              // 返回 gpus 里的下标；独显优先，其次显存大、计算单元多
bool cpuHasAvx2();
QString describe(const GpuInfo &g);
}
