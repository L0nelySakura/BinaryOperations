#pragma once

#include <QObject>
#include <QStringList>
#include <QtGlobal>

#include "fileprocessor.h"
#include "settings.h"

class FileManager;

class Worker : public QObject {
  Q_OBJECT

 public:
  explicit Worker(FileManager* file_manager, const Settings& settings,
                  QObject* parent = nullptr);

  void process();

 public slots:
  void request_cancel();

 signals:
  void progress_overall(int percent);
  void progress_file(const QString& file_name, int percent);
  void status_message(const QString& message);
  void finished();
  void error_occurred(const QString& message);

 private:
  FileManager* file_manager_;
  const Settings& settings_;
  QAtomicInt cancel_requested_{0};
};
