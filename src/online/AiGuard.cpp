#include "AiGuard.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace {
// 常见的围棋 AI 引擎和带 AI 的客户端（进程名里包含这些就算）
const char *const kPatterns[] = {
    "katago", "leelaz", "leela-zero", "leela_zero", "leela", "gnugo", "pachi", "katrain", "sabaki",
    "lizzie", "lizgoban", "goreviewpartner", "ogatak", "kata-", "golaxy", "fineart", "elfgo", "minigo",
};

bool suspicious(const QString &exe)
{
    const QString n = exe.toLower();
    for (const char *p : kPatterns)
        if (n.contains(QLatin1String(p))) return true;
    // 另开的 NanoGo 客户端；nanogo-server 等其他程序不算
    return n == QLatin1String("nanogo") || n == QLatin1String("nanogo.exe");
}
}

QStringList AiGuard::findGoAi()
{
    QSet<QString> found;
    const qint64 self = QCoreApplication::applicationPid();
#ifdef Q_OS_WIN
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return {};
    PROCESSENTRY32W e;
    e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        if (qint64(e.th32ProcessID) == self) continue;
        const QString exe = QString::fromWCharArray(e.szExeFile);
        if (suspicious(exe)) found.insert(exe);
    }
    CloseHandle(snap);
#else
    for (const QString &pid : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool num = false;
        const qint64 p = pid.toLongLong(&num);
        if (!num || p == self) continue;
        QFile f("/proc/" + pid + "/comm");
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString exe = QString::fromLocal8Bit(f.readAll()).trimmed();
        if (suspicious(exe)) found.insert(exe);
    }
#endif
    QStringList list = found.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}
