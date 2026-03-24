#include "../ui/main_window.h"
#include <QApplication>
#include <QFile>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char *argv[])
{

    spdlog::set_default_logger(spdlog::stdout_color_mt("console"));

    QApplication app(argc, argv);
    QFile file(":/src/style/style.qss");
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QString styleSheet = QLatin1String(file.readAll());
        app.setStyleSheet(styleSheet);
        file.close();
    }
    MainWindow w;
    w.show();
    return app.exec();
}