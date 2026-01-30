#include "filemanager.h"

#include <QDir>
#include <QFileInfo>

FileManager::FileManager(QObject* parent) : QObject(parent) {}

bool FileManager::set_input_directory(const QString& directory_path) {
  if (!directory_exists(directory_path)) {
    emit error_occurred("Входная папка не существует: " + directory_path);
    input_directory_.clear();
    return false;
  }
  input_directory_ = directory_path;
  return true;
}

void FileManager::set_output_directory(const QString& directory_path) {
  if (!directory_path.isEmpty() && !directory_exists(directory_path)) {
    emit error_occurred("Выходная папка не существует: " + directory_path);
    output_directory_.clear();
    return;
  }
  output_directory_ = directory_path;
}

void FileManager::set_file_mask(const QString& file_mask) {
  if (!mask_is_valid(file_mask)) {
    emit error_occurred("Некорректная маска файлов: " + file_mask);
    file_mask_.clear();
    return;
  }
  file_mask_ = file_mask;
}

bool FileManager::is_valid() const {
  return !input_directory_.isEmpty() && !file_mask_.isEmpty();
}

QStringList FileManager::input_files() {
  if (!is_valid()) {
    emit error_occurred(
        "Невозможно получить список файлов: параметры не заданы");
    return {};
  }

  QDir directory(input_directory_);
  QStringList files = directory.entryList({file_mask_},
                                          QDir::Files | QDir::Readable,
                                          QDir::Name);

  if (files.isEmpty()) {
    emit error_occurred("Файлы по заданной маске не найдены");
  }

  for (QString& file : files) {
    file = directory.absoluteFilePath(file);
  }

  return files;
}

QString FileManager::get_output_path_for(const QString& input_file_path,
                                         const QString& output_directory,
                                         OutputPathMode path_mode) const {
  QFileInfo input_info(input_file_path);
  QString base_name = input_info.fileName();
  QString out_path = output_directory + QDir::separator() + base_name;

  if (path_mode != OutputPathMode::kAppendCounter) {
    return out_path;
  }

  QString name = input_info.completeBaseName();
  QString suffix = input_info.suffix();
  const QDir out_dir(output_directory);
  int counter = 1;
  while (true) {
    QString candidate = suffix.isEmpty()
                           ? QString("%1_%2").arg(name).arg(counter)
                           : QString("%1_%2.%3").arg(name).arg(counter).arg(suffix);
    QString candidate_path = out_dir.absoluteFilePath(candidate);
    if (!QFileInfo::exists(candidate_path)) {
      return candidate_path;
    }
    ++counter;
  }
}

bool FileManager::directory_exists(const QString& path) const {
  QFileInfo info(path);
  return info.exists() && info.isDir();
}

bool FileManager::mask_is_valid(const QString& mask) const {
  if (mask.trimmed().isEmpty()) return false;
  return mask.contains('*') || mask.contains('.');
}
