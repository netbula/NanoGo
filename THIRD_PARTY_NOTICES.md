# Third-party notices / 第三方组件声明

NanoGo itself is released under the MIT License (see `LICENSE`).
The Windows release package bundles or downloads the following third-party components.
Full license texts are in the `licenses/` folder of the release package.

| Component | Version | License | How it is used |
|---|---|---|---|
| [Qt](https://www.qt.io/) (QtCore, QtGui, QtWidgets, QtNetwork, platform plugins) | 6.8.3 | LGPL-3.0 | Dynamically linked DLLs shipped next to `NanoGo.exe`. You may replace them with your own build of Qt 6.8.x. Qt source code: https://download.qt.io/official_releases/qt/6.8/6.8.3/single/ |
| [KataGo](https://github.com/lightvector/KataGo) engine (OpenCL and CPU/Eigen builds) | v1.18.1 | MIT (plus licenses of its bundled libraries, see KataGo's `LICENSE` and `cpp/external`) | Unmodified official release binaries in `katago/`, run as a separate process. |
| Libraries shipped inside the KataGo release (OpenSSL, zlib, bzip2, libzip, CLBlast, ...) | as shipped by KataGo | Apache-2.0 / zlib / BSD-style | Unmodified, inside `katago/`. |
| Microsoft Visual C++ runtime (`vcruntime140*.dll`, `msvcp140*.dll`) | VS 2022 | Microsoft Visual Studio "Distributable Code" | App-local runtime DLLs. |
| KataGo neural network models from [katagotraining.org](https://katagotraining.org/) | latest strongest at first run | MIT-style license from the KataGo training project | **Not bundled.** Downloaded on first run directly from the official site and verified with SHA-256. |

GPU drivers / OpenCL runtimes are not bundled; they come with your graphics driver. If no usable GPU is found,
NanoGo automatically switches to the CPU build of KataGo.

Go is a game with no single owner; the board, stones and rules are not anyone's trademark. NanoGo is not affiliated with
KataGo, katagotraining.org or the Qt Company.
