#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class FileManager : public QObject {
  Q_OBJECT

 public:
  explicit FileManager(QObject* parent = nullptr);

  bool set_input_directory(const QString& directory_path);
  void set_output_directory(const QString& directory_path);
  void set_file_mask(const QString& file_mask);

  const QString& input_directory() const { return input_directory_; }
  const QString& output_directory() const { return output_directory_; }
  const QString& file_mask() const { return file_mask_; }

  bool is_valid() const;
  QStringList input_files();

 signals:
  void error_occurred(const QString& message);

 private:
  bool directory_exists(const QString& path) const;
  bool mask_is_valid(const QString& mask) const;

  QString input_directory_;
  QString output_directory_;
  QString file_mask_;
};
