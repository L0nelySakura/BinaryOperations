#pragma once

#include <QMainWindow>
#include <QScopedPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "filemanager.h"
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Worker;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

  void on_browse_input_directory();
  void on_browse_output_directory();

 signals:
  void stop_requested();

 private:
    void connect_ui_to_settings();
    void on_start_stop_clicked();
    void start_worker_with_list(const QStringList& files);
    void on_run_timer();
    void on_scan_timer();
    void start_timers_if_periodic();
    void stop_timers();
    void on_worker_finished();
    void on_worker_progress_overall(int percent);
    void on_worker_progress_file(const QString& file_name, int percent);
    void on_worker_status_message(const QString& message);
    void on_worker_error(const QString& message);
    static QByteArray parse_hex_to_8_bytes(const QString& hex_string);

    Ui::MainWindow* ui;
    Settings settings_;
    Settings active_settings_;
    bool has_active_settings_ = false;
    QScopedPointer<FileManager> file_manager_;
    Worker* worker_ = nullptr;
    QThread* worker_thread_ = nullptr;
    QStringList pending_files_;
    QTimer* run_timer_ = nullptr;
    QTimer* scan_timer_ = nullptr;
};
