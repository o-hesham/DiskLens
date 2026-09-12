#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("DiskLens"));
    application.setApplicationVersion(QStringLiteral(DISKLENS_VERSION));

    MainWindow mainWindow;
    mainWindow.show();

    return application.exec();
}
