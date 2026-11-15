# Third-party notices / 第三方组件声明

NanoGo itself is released under the MIT License (see `LICENSE`).
The Windows release package bundles or downloads the following third-party components.
Full license texts are in the `licenses/` folder of the release package.

| Component | Version | License | How it is used |
|---|---|---|---|
| [Qt](https://www.qt.io/) (QtCore, QtGui, QtWidgets, QtNetwork, platform plugins) | 6.8.3 | LGPL-3.0 | Dynamically linked DLLs shipped next to `NanoGo.exe`. You may replace them with your own build of Qt 6.8.x. Qt source code: https://download.qt.io/official_releases/qt/6.8/6.8.3/single/ |
| [KataGo](https://github.com/lightvector/KataGo) engine (OpenCL or CPU/Eigen build) | v1.18.1 | MIT, plus the licenses of the libraries inside KataGo's release zip (OpenSSL 3 Apache-2.0, zlib, bzip2, libzip) | **Not bundled.** On first run NanoGo downloads the official release zip that matches the PC from github.com/lightvector/KataGo, verifies its SHA-256 and unpacks it unmodified into `katago/`. It runs as a separate process. |
| Microsoft Visual C++ runtime (`vcruntime140*.dll`, `msvcp140*.dll`) | VS 2022 | Microsoft Visual Studio "Distributable Code" | App-local runtime DLLs. |
| KataGo neural network models from [katagotraining.org](https://katagotraining.org/) | latest strongest at first run | MIT-style license from the KataGo training project | **Not bundled.** Downloaded on first run directly from the official site and verified with SHA-256. |

GPU drivers / OpenCL runtimes are not bundled; they come with your graphics driver. If no usable GPU is found,
NanoGo automatically switches to the CPU build of KataGo.

Go is a game with no single owner; the board, stones and rules are not anyone's trademark. NanoGo is not affiliated with
KataGo, katagotraining.org or the Qt Company.

NanoGo 的程序图标和工具栏图示都由 NanoGo 自己用代码绘制（`src/Icons.cpp`），随 NanoGo 一起按 MIT 许可证发布，没有使用第三方图标。
The application icon and toolbar icons are drawn in code by NanoGo itself (`src/Icons.cpp`) and are MIT licensed with NanoGo; no third-party icon sets are used.
