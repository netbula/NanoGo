NanoGo 围棋 / NanoGo Go board
==============================

【中文】
1. 把整个压缩包解压到一个普通文件夹（例如 桌面 或 D:\NanoGo），不要放在 C:\Program Files 里。
2. 双击 NanoGo.exe。
3. 第一次打开会自动完成（需要联网）：
   - 识别显卡，从 KataGo 官方 GitHub 下载合适的引擎：有显卡下载显卡版，没有就下载 CPU 版（约 6 MB）；
   - 下载 KataGo 官方最强的神经网络模型（约 250 MB，只下载一次）；
   - 第一次在显卡上运行时自动调优，有进度条；
   - 自动测试本机硬件，选出最合适的设置（1 到 3 分钟）；
   - 有独立显卡就用显卡，没有就自动改用 CPU 版。
4. 菜单“设置 > 界面语言”可以切换中文 / English / 日本語 / 한국어。
5. 想和 AI 下得有来有回：工具栏“AI 强度”里选“人类 X 级/段”，第一次选会下载约 95 MB 的人类风格模型。
6. 复盘：点棋盘下方“分析整盘棋”，看胜率曲线和红/橙色标出的坏棋。

【English】
1. Unzip the whole package into a normal folder (e.g. Desktop or D:\NanoGo), not C:\Program Files.
2. Double-click NanoGo.exe.
3. On first start (internet needed) NanoGo detects your graphics card and downloads the matching official
   KataGo engine (GPU/OpenCL or CPU build, ~6 MB), then the strongest official KataGo network (~250 MB, once),
   test your hardware to pick the best settings (1-3 minutes), and use your GPU if available or the CPU build otherwise.
4. Settings > Interface language switches between 中文 / English / 日本語 / 한국어.
5. For a beatable opponent pick "Human Xk/Xd" under AI strength (downloads a ~95 MB human-style model once).
6. Review: click "Analyze whole game" below the board for the win-rate graph and marked mistakes.

Licenses: see LICENSE, THIRD_PARTY_NOTICES.md and the licenses folder.
