# NanoGo 围棋

一个用 C++ / Qt 6 写的 KataGo 图形界面，支持中文、English、日本語、한국어 四种界面语言（菜单“设置 > 界面语言”），用来复盘、对弈和做死活题。

## 当前功能（第二步：程序骨架）
- 19 路棋盘，落子、提子、打劫判断
- 打开 / 保存 SGF 棋谱（自动识别 UTF-8 和 GBK 编码），支持变化分支
- 通过 KataGo analysis 协议实时分析：胜率、领先目数、候选点（悬停看变化图）、形势判断
- 自动寻找 `C:\dev\NanoGo\katago` 里的 KataGo 引擎（优先 OpenCL 版）和模型（优先 zhizi），不依赖 KaTrain
- 引擎出错时给出中文说明，并可直接打开设置
- 状态栏显示正在使用哪块显卡
- 自动跟踪官方最强模型：每天从 katagotraining.org 读取模型列表，按评分可信下限挑出最强的，下载到 `C:\dev\NanoGo\models`（校验 SHA-256）并自动切换；菜单“设置 > 检查模型更新…”可手动检查
- 参考图面板：列出 AI 推荐的几手（胜率、目差、变化），点一下在棋盘上用数字显示整条变化；并评价上一手和 AI 推荐差多少目
- 自动保存：每走一步都保存到 `data\autosave.sgf`，重新打开时恢复棋局、位置和人机对弈状态
- 人机对弈（基础版）：选执黑或执白、让子 0 到 9 子、AI 每步计算量；双方停一手结束，AI 大劣时认输

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

## 编译（Windows）
需要：Visual Studio 2026（含 C++ 桌面开发）、Qt 6.8.3 msvc2022_64（装在 `C:\dev\Qt`）。

```bat
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% --preset msvc
%CMAKE% --build --preset release
```

生成的程序在 `build\Release\NanoGo.exe`，Qt 运行库会自动复制到同一目录。`CMakePresets.json` 里的 Qt 路径按你自己的安装位置修改。
自己编译时不用准备 KataGo，第一次运行会自动下载；也可以自己把 KataGo 官方发布的 Windows 版解压到 `katago\opencl`、`katago\cpu` 等子文件夹。

推送 `v*` 标签时，GitHub Actions（`.github/workflows/release.yml`）会自动编译、打包并发布到 Releases。

## 文件位置
- `nanogo.ini`：设置，放在 exe 旁边
- `data\`：程序生成的 KataGo 配置、显卡调优文件和日志 `nanogo.log`（遇到问题可以把这个文件发给开发者）
- 程序不会往“下载”文件夹里写任何东西

## 硬件自动检测
- 不针对某一台电脑写死设置。第一次启动（或更换引擎、模型后），NanoGo 会用 KataGo 自带的 benchmark 自动测试，选出这台电脑最合适的搜索线程数，结果记在 `nanogo.ini`。设置里可以“重新测试硬件”，也可以手动指定线程数。
- 显卡默认“自动”：NanoGo 自己列出所有显卡，优先独立显卡、显存大的那块，并核对 KataGo 实际用的是哪块。
- 引擎按 OpenCL → TensorRT → CUDA → CPU（eigen）的顺序在 `katago` 文件夹里自动寻找；没有独立显卡的电脑放 CPU 版即可。
- 路径都跟随程序所在位置：从 exe 往上找含 `katago` 或 `models` 文件夹的目录作为根目录，整个文件夹拷到别的电脑也能用。
