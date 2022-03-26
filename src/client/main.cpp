#include <QApplication>
#include <QDirIterator>
#include <QDebug>

#include <winsock2.h>
#include "dropsitewindow.h"

//! [main() function]
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    DropSiteWindow window;
    window.show();
    return app.exec();
}
//! [main() function]
