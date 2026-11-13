#include "MainWindow.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("NanoGo");
    QApplication::setStyle("Fusion");
    MainWindow w;
    w.show();

    // 开发用：设置环境变量 NANOGO_SCREENSHOT=文件名，程序会自动截图后退出
    const QString shot = qEnvironmentVariable("NANOGO_SCREENSHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(2500, &w, [&w, shot] {
            w.grab().save(shot);
            QApplication::quit();
        });
    }
    return app.exec();
}
