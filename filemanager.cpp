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

bool FileManager::directory_exists(const QString& path) const {
  QFileInfo info(path);
  return info.exists() && info.isDir();
}

bool FileManager::mask_is_valid(const QString& mask) const {
  if (mask.trimmed().isEmpty()) return false;
  return mask.contains('*') || mask.contains('.');
}
