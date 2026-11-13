# NanoGo 围棋

一个用 C++ / Qt 6 写的 KataGo 图形界面，中文界面，用来复盘、对弈和做死活题。

## 当前功能（第二步：程序骨架）
- 19 路棋盘，落子、提子、打劫判断
- 打开 / 保存 SGF 棋谱（自动识别 UTF-8 和 GBK 编码），支持变化分支
- 通过 KataGo analysis 协议实时分析：胜率、领先目数、候选点（悬停看变化图）、形势判断
- 自动寻找 KataGo（优先 OpenCL 版）和模型（优先 zhizi）
- 引擎出错时给出中文说明，并可直接打开设置
- 状态栏显示正在使用哪块显卡

## 快捷键
| 按键 | 作用 |
|---|---|
| 空格 | 开始 / 停止分析 |
| ← / → | 后退 / 前进 |
| Home / End | 回到开头 / 走到最后 |
| H | 显示 / 隐藏候选点 |
| T | 显示 / 隐藏形势 |
| Ctrl+P | 停一手 |

## 编译（Windows）
需要：Visual Studio 2026（含 C++ 桌面开发）、Qt 6.8.3 msvc2022_64（装在 `C:\dev\Qt`）。

```bat
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% --preset msvc
%CMAKE% --build --preset release
```

生成的程序在 `build\Release\NanoGo.exe`，Qt 运行库会自动复制到同一目录。

## 文件位置
- `nanogo.ini`：设置，放在 exe 旁边
- `data\`：程序生成的 KataGo 配置 `analysis.cfg` 和显卡调优文件
- 程序不会往“下载”文件夹里写任何东西

## 显卡说明（RTX 5060 Ti）
- 推荐 KataGo **OpenCL** 版，搜索线程 16（实测 zhizi b40 约 216 次/秒）
- 这台电脑 OpenCL 0 号是 AMD 集成显卡，1 号才是 5060 Ti。设置里“显卡”选“自动”即可，KataGo 会挑 5060 Ti
