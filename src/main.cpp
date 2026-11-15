#include "AppSettings.h"
#include "I18n.h"
#include "MainWindow.h"
#include "Icons.h"
#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QTimer>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(Icons::app()); // 窗口标题栏、任务栏图标
    QApplication::setApplicationName("NanoGo");
    QApplication::setStyle("Fusion");
    // 开发用：NANOGO_EXPORT_ICON=文件夹，导出程序图标的 PNG（用来生成 res/nanogo.ico）后退出
    const QString iconDir = qEnvironmentVariable("NANOGO_EXPORT_ICON");
    if (!iconDir.isEmpty()) {
        for (int size : {16, 24, 32, 48, 64, 128, 256})
            Icons::app().pixmap(size, size).save(QDir(iconDir).filePath(QStringLiteral("icon_%1.png").arg(size)));
        return 0;
    }

    // 界面语言：设置里选的，或者跟随系统
    AppSettings s;
    s.load();
    const QString lang = qEnvironmentVariableIsSet("NANOGO_LANG") ? qEnvironmentVariable("NANOGO_LANG") : s.effectiveLanguage();
    I18n::setLanguage(lang);
    // Qt 自带对话框按钮（是/否/确定）的翻译：windeployqt 会把 translations 放到 exe 旁边
    QTranslator qtTr;
    const QString qtLang = lang == "zh" ? QStringLiteral("zh_CN") : lang;
    if (qtTr.load("qtbase_" + qtLang, QDir(AppSettings::appDir()).filePath("translations"))
        || qtTr.load("qtbase_" + qtLang, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTr);

    MainWindow w;
    w.show();

    // 开发用：设置环境变量 NANOGO_SCREENSHOT=文件名，程序会自动截图后退出
    const QString shot = qEnvironmentVariable("NANOGO_SCREENSHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("NANOGO_SCREENSHOT_DELAY") > 0 ? qEnvironmentVariableIntValue("NANOGO_SCREENSHOT_DELAY") : 2500, &w, [&w, shot] {
            w.grab().save(shot);
            QApplication::quit();
        });
    }
    return app.exec();
}
