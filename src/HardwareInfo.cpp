#include "HardwareInfo.h"
#include "I18n.h"
#include <QLibrary>

#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace {
// 只用到 OpenCL 的几个函数，运行时动态加载，没有 OpenCL 驱动也不会启动失败
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long long cl_ulong;
typedef cl_ulong cl_bitfield;
typedef void *cl_platform_id;
typedef void *cl_device_id;
const cl_bitfield CL_DEVICE_TYPE_GPU = 1 << 2;
const cl_bitfield CL_DEVICE_TYPE_ALL = 0xFFFFFFFF;
const cl_uint CL_DEVICE_TYPE_INFO = 0x1000;
const cl_uint CL_DEVICE_MAX_COMPUTE_UNITS = 0x1002;
const cl_uint CL_DEVICE_GLOBAL_MEM_SIZE = 0x101F;
const cl_uint CL_DEVICE_HOST_UNIFIED_MEMORY = 0x1035;
const cl_uint CL_DEVICE_NAME = 0x102B;
const cl_uint CL_DEVICE_VENDOR = 0x102C;

#ifdef _WIN32
#define CLAPI __stdcall
#else
#define CLAPI
#endif
typedef cl_int(CLAPI *PFN_clGetPlatformIDs)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int(CLAPI *PFN_clGetDeviceIDs)(cl_platform_id, cl_bitfield, cl_uint, cl_device_id *, cl_uint *);
typedef cl_int(CLAPI *PFN_clGetDeviceInfo)(cl_device_id, cl_uint, size_t, void *, size_t *);
} // namespace

QList<GpuInfo> HardwareInfo::openclGpus(QString *error)
{
    QList<GpuInfo> out;
    QLibrary lib("OpenCL");
    if (!lib.load()) {
        if (error) *error = T("没有找到 OpenCL 驱动（OpenCL.dll）。请安装或更新显卡驱动；在此之前会使用 CPU 版。");
        return out;
    }
    auto getPlatforms = reinterpret_cast<PFN_clGetPlatformIDs>(lib.resolve("clGetPlatformIDs"));
    auto getDevices = reinterpret_cast<PFN_clGetDeviceIDs>(lib.resolve("clGetDeviceIDs"));
    auto getInfo = reinterpret_cast<PFN_clGetDeviceInfo>(lib.resolve("clGetDeviceInfo"));
    if (!getPlatforms || !getDevices || !getInfo) {
        if (error) *error = T("OpenCL 驱动不完整。请更新显卡驱动。");
        return out;
    }
    cl_uint np = 0;
    if (getPlatforms(0, nullptr, &np) != 0 || np == 0) {
        if (error) *error = T("显卡驱动没有提供 OpenCL 支持。请安装或更新显卡驱动；在此之前会使用 CPU 版。");
        return out;
    }
    QVector<cl_platform_id> platforms(np);
    getPlatforms(np, platforms.data(), nullptr);
    int index = 0; // 和 KataGo 一样：按平台顺序把所有设备依次编号
    for (cl_platform_id p : platforms) {
        cl_uint nd = 0;
        if (getDevices(p, CL_DEVICE_TYPE_ALL, 0, nullptr, &nd) != 0 || nd == 0) continue;
        QVector<cl_device_id> devs(nd);
        getDevices(p, CL_DEVICE_TYPE_ALL, nd, devs.data(), nullptr);
        for (cl_device_id d : devs) {
            GpuInfo g;
            g.clIndex = index++;
            char buf[512] = {0};
            getInfo(d, CL_DEVICE_NAME, sizeof(buf) - 1, buf, nullptr);
            g.name = QString::fromUtf8(buf).trimmed();
            memset(buf, 0, sizeof(buf));
            getInfo(d, CL_DEVICE_VENDOR, sizeof(buf) - 1, buf, nullptr);
            g.vendor = QString::fromUtf8(buf).trimmed();
            cl_ulong mem = 0;
            getInfo(d, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem), &mem, nullptr);
            g.memoryMB = mem >> 20;
            cl_uint unified = 0;
            getInfo(d, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(unified), &unified, nullptr);
            g.integrated = unified != 0;
            cl_uint cu = 0;
            getInfo(d, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, nullptr);
            g.computeUnits = int(cu);
            cl_bitfield type = 0;
            getInfo(d, CL_DEVICE_TYPE_INFO, sizeof(type), &type, nullptr);
            if (type & CL_DEVICE_TYPE_GPU) out.append(g); // 只要显卡；编号仍按全部设备计算
        }
    }
    if (out.isEmpty() && error) *error = T("没有找到可用于计算的显卡，会使用 CPU 版。");
    return out;
}

int HardwareInfo::bestGpu(const QList<GpuInfo> &gpus)
{
    int best = -1;
    auto score = [](const GpuInfo &g) {
        double s = 0;
        if (!g.integrated) s += 1e9;                     // 独立显卡优先
        const QString v = g.vendor.toLower() + " " + g.name.toLower();
        if (v.contains("nvidia")) s += 1e8;              // NVIDIA 卡的 OpenCL 一般最稳
        s += double(g.memoryMB) * 1000 + g.computeUnits; // 显存大的优先
        return s;
    };
    for (int i = 0; i < gpus.size(); ++i)
        if (best < 0 || score(gpus[i]) > score(gpus[best])) best = i;
    return best;
}

QList<DisplayAdapter> HardwareInfo::displayAdapters()
{
    QList<DisplayAdapter> out;
#ifdef _WIN32
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))) return out;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            DisplayAdapter a;
            a.name = QString::fromWCharArray(desc.Description);
            a.vendorId = desc.VendorId;
            a.dedicatedMB = quint64(desc.DedicatedVideoMemory) >> 20;
            a.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (!a.software) out.append(a);
        }
        adapter->Release();
    }
    factory->Release();
#endif
    return out;
}

bool HardwareInfo::cpuHasAvx2()
{
#ifdef _WIN32
    int info[4];
    __cpuid(info, 0);
    if (info[0] < 7) return false;
    __cpuidex(info, 7, 0);
    const bool avx2 = (info[1] & (1 << 5)) != 0;
    __cpuid(info, 1);
    const bool osxsave = (info[2] & (1 << 27)) != 0;
    return avx2 && osxsave;
#else
    unsigned a, b, c, d;
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d)) return false;
    return (b & (1 << 5)) != 0;
#endif
}

QString HardwareInfo::describe(const GpuInfo &g)
{
    return T("%1: %2（%3 MB，%4）").arg(g.clIndex).arg(g.name).arg(g.memoryMB)
        .arg(g.integrated ? T("集成显卡") : T("独立显卡"));
}
