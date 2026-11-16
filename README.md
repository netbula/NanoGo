# NanoGo 围棋

一个用 C++ / Qt 6 写的围棋复盘、对弈图形界面，支持中文、English、日本語、한국어 四种界面语言（菜单“设置 > 界面语言”）。

**NanoGo 的棋力全部来自 [KataGo](https://github.com/lightvector/KataGo)。** 本程序使用 KataGo 官方原版引擎和 [katagotraining.org](https://katagotraining.org/) 的官方模型，未做修改，只是为它们提供界面。NanoGo 与 KataGo 项目没有隶属关系。

## 功能
- 19 路棋盘，落子、提子、打劫判断
- 打开 / 保存 SGF 棋谱（自动识别 UTF-8 和 GBK 编码），支持变化分支
- 通过 KataGo analysis 协议实时分析：胜率、领先目数、候选点（悬停看变化图）、形势判断
- 第一次打开自动下载 KataGo 引擎和模型，自动识别显卡、调优、测速（都有进度条）
- 引擎出错时给出中文说明，并可直接打开设置
- 状态栏显示正在使用哪块显卡
- 自动跟踪官方最强模型：每天从 katagotraining.org 读取模型列表，按评分可信下限挑出最强的，下载到程序旁边的 `models` 文件夹（校验 SHA-256）并自动切换；也可以在“设置 > 选择模型…”里自己挑任意官方模型（最强只是默认）
- 参考图面板：列出 AI 推荐的几手（胜率、目差、变化），点一下在棋盘上用数字显示整条变化；并评价上一手和 AI 推荐差多少目
- 自动保存：每走一步都保存到 `data\autosave.sgf`，重新打开时恢复棋局、位置和人机对弈状态
- 人机对弈：选执黑或执白、让子 0 到 9 子；对局中随时“交换黑白”、随时调整“AI 强度”；双方停一手后数子，AI 大劣时认输
- 按人类段位下棋：AI 强度里选“野狐 18 级 … 9 段（按野狐围棋的段级位），以及“职业 2023 年 … 1930 年”（模仿那一年职业棋谱的下法）”，AI 用 KataGo 官方的人类风格模型（b18c384nbt-humanv0，第一次选时自动下载约 95 MB）模仿该水平的人类下法
- 复盘：“分析整盘棋”算出每一手的胜率和目差，棋盘下方画胜率曲线（点击跳到那一手），自动标出恶手（红）和疑问手（橙），PgUp / PgDn 在坏棋之间跳转
- 死活题练习（Ctrl+L）：棋盘自动放大到题目区域，你下一手、AI 替对方应，对错由 KataGo 现场判断（比较题目区域内棋子的死活），所以任何 SGF 死活题都能做；自带一组基础题，也可以“打开题库”载入自己的 SGF 题目文件，进度自动保存
- 死活题编辑器（棋局菜单 > 死活题编辑器…）：摆黑白子、设谁先走和目标（活/杀/劫/吃子）、增删和排序题目，存成标准 SGF 题库；“用 KataGo 检查”会找出正解第一手并确认题目成立；编好点“做这套题”直接练
- 在线教学（可选，“在线”菜单）：登录老师或机构架设的 NanoGo 教学服务器后，可以把棋谱存到云端，学生做老师布置的作业（死活题、复盘、和 AI 下棋）并提交，老师查看和批改，管理员管理账号和班级。不登录时以上功能都不出现，NanoGo 照常单机使用。服务端见 [server/README.md](server/README.md)

## 快捷键
| 按键 | 作用 |
|---|---|
| 空格 | 开始 / 停止分析 |
| ← / → | 后退 / 前进（人机对弈时 ← 是悔棋，一次退两步） |
| Ctrl+Enter | 让 AI 替当前一方走一步（不管轮到谁） |
| Home / End | 回到开头 / 走到最后 |
| H | 显示 / 隐藏候选点 |
| T | 显示 / 隐藏形势 |
| Ctrl+P | 停一手 |
| Ctrl+G | 人机对弈 |
| Ctrl+T | 交换黑白（和 AI 互换执子颜色） |
| Ctrl+E | 点目 / 结束对局 |
| V | 参考图：在棋盘上显示 AI 推荐的第一条变化 |

## 下载使用（推荐）
到 [Releases](../../releases) 下载 `NanoGo-版本号-windows-x64.zip`，解压到普通文件夹，双击 `NanoGo.exe` 即可。
不需要另外安装任何东西：
- Qt 运行库、VC 运行库在压缩包里；KataGo 引擎不打包，第一次打开时按本机硬件从 KataGo 官方 GitHub 下载对应版本（显卡 OpenCL 版，或按 CPU 是否支持 AVX2 选 CPU 版），版本和 SHA-256 写死在程序里校验；
- 第一次打开自动下载 KataGo 官方最强模型（只下载一次，SHA-256 校验）；
- 自动识别显卡：自己列出所有显卡，优先选独立显卡、显存大的那块，明确告诉 KataGo，不让它在集成显卡和独立显卡之间猜；没有可用显卡或显卡驱动不支持 OpenCL 时自动改用 CPU 版（按 CPU 是否支持 AVX2 选择）；
- 自动测速，选出这台电脑最合适的线程数。

授权：NanoGo 本身是 MIT 许可（见 `LICENSE`），第三方组件见 `THIRD_PARTY_NOTICES.md`。

## 系统和硬件要求

**能运行的系统**
- Windows 11（64 位）
- Windows 10 64 位，1809 版（2018 年底）或更新的版本

**不能运行**
- Windows 7、8、8.1：界面库 Qt 6 已不支持这些系统
- 32 位 Windows：KataGo 官方只提供 64 位版本
- ARM 处理器的 Windows 电脑（如部分骁龙笔记本）：没有原生版本，Windows 11 的 x64 模拟也许能打开，但会很慢，未测试

**硬件**
| | 最低 | 建议 |
|---|---|---|
| 显卡 | 不需要（自动改用 CPU，较慢） | NVIDIA / AMD 独立显卡，装好显卡驱动（需支持 OpenCL） |
| CPU | 64 位 x86 CPU | 支持 AVX2（约 2013 年后的 Intel、2015 年后的 AMD），没有显卡时快很多 |
| 内存 | 4 GB | 8 GB 以上 |
| 硬盘 | 约 400 MB | 用野狐段位或职业年份 AI 时另需约 99 MB |

**网络**
- 第一次打开需要联网，自动下载 KataGo 引擎（约 6 MB）和模型（约 262 MB）
- 第一次选“野狐段位”或“职业年份”AI 时，会再下载人类风格模型（约 99 MB）
- 下载完成后不联网也能下棋、分析和做题

**说明**
- 有显卡时，第一次使用会为这块显卡自动调优约 5 分钟，只做一次。有多块显卡时自动选独立显卡、显存大的一块，也可以在设置里手动换。
- 不用另外安装 Qt、VC++ 运行库或 KataGo，都已包含或会自动下载。
- 实际测试过的配置：Windows 11 + NVIDIA RTX 5060 Ti（另有 AMD 集成显卡）。其他配置按 Qt 和 KataGo 官方支持范围推算，遇到问题请把 `data\nanogo.log` 发给开发者。

## 编译（Windows）
需要：Visual Studio 2022 或更新版本（含 C++ 桌面开发）、Qt 6.8.x msvc2022_64。

```bat
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% --preset msvc
%CMAKE% --build --preset release
```

生成的程序在 `build\Release\NanoGo.exe`，Qt 运行库会自动复制到同一目录。`CMakePresets.json` 里的 Qt 路径按你自己的安装位置修改。
自己编译时不用准备 KataGo，第一次运行会自动下载；也可以自己把 KataGo 官方发布的 Windows 版解压到 `katago\opencl`、`katago\cpu` 等子文件夹。

在 GitHub 上发布一个 Release（例如 `v0.1.0`）时，GitHub Actions（`.github/workflows/release.yml`）会自动编译、打包，并把安装包附到这个 Release 上。

## 文件位置
- `nanogo.ini`：设置，放在 exe 旁边
- `data\`：程序生成的 KataGo 配置、显卡调优文件和日志 `nanogo.log`（遇到问题可以把这个文件发给开发者）
- 程序不会往“下载”文件夹里写任何东西

## 硬件自动检测
- 不针对某一台电脑写死设置。第一次启动（或更换引擎、模型后），NanoGo 会用 KataGo 自带的 benchmark 自动测试，选出这台电脑最合适的搜索线程数，结果记在 `nanogo.ini`。设置里可以“重新测试硬件”，也可以手动指定线程数。
- 显卡默认“自动”：NanoGo 自己列出所有显卡，优先独立显卡、显存大的那块，并核对 KataGo 实际用的是哪块。
- 引擎按 OpenCL → TensorRT → CUDA → CPU（eigen）的顺序在 `katago` 文件夹里寻找，找不到就按硬件自动下载合适的官方版本。
- 路径都跟随程序所在位置：从 exe 往上找含 `katago` 或 `models` 文件夹的目录作为根目录，整个文件夹拷到别的电脑也能用。

## 致谢
- [KataGo](https://github.com/lightvector/KataGo)（David J. Wu 及贡献者，MIT）：围棋引擎
- [katagotraining.org](https://katagotraining.org/)：官方神经网络模型和分布式训练
- [Qt](https://www.qt.io/)（LGPL-3.0）：界面库
- [Lucide](https://lucide.dev)（ISC）：工具栏图示
