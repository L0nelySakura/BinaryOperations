#pragma once

#include <QMainWindow>
#include <QScopedPointer>

#include "filemanager.h"
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(Settings& settings, QWidget* parent = nullptr);
  ~MainWindow();

  void on_browse_input_directory();
  void on_browse_output_directory();

 private:
  void connect_ui_to_settings();
  static QByteArray parse_hex_to_8_bytes(const QString& hex_string);

  Ui::MainWindow* ui;
  Settings& settings_;
  QScopedPointer<FileManager> file_manager_;
};
