#include "mainwindow.h"

#include <QApplication>

#include "settings.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  Settings settings;
  MainWindow main_window(settings);
  main_window.show();

  return app.exec();
}
