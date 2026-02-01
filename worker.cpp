#include "worker.h"

#include "filemanager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

Worker::Worker(FileManager* file_manager, const Settings& settings,
               QObject* parent)
    : QObject(parent), file_manager_(file_manager), settings_(settings) {}

void Worker::request_cancel() {
  cancel_requested_.storeRelaxed(1);
}

void Worker::set_files_to_process(const QStringList& paths) {
  files_to_process_ = paths;
}

void Worker::process() {
    if (!file_manager_->is_valid()) {
        emit error_occurred("Не заданы входная папка или маска файлов.");
        emit finished();
        return;
    }

    if (settings_.xor_key_8_bytes().size() != 8) {
        emit error_occurred("Ключ XOR должен быть ровно 8 байт (16 hex-символов).");
        emit finished();
        return;
    }

    if (settings_.output_directory().isEmpty()) {
        emit error_occurred("Не указана выходная папка.");
        emit finished();
        return;
    }

    QStringList input_paths =
          files_to_process_.isEmpty() ? file_manager_->input_files()
                                      : files_to_process_;
    if (input_paths.isEmpty()) {
        emit status_message("Нет файлов для обработки.");
        emit finished();
        return;
    }

    const int total_files = input_paths.size();
    const bool delete_input = settings_.delete_input_files();
    const QByteArray xor_key = settings_.xor_key_8_bytes();
    const FileManager::OutputPathMode path_mode =
        settings_.output_name_conflict() == Settings::OutputNameConflict::kOverwrite
                                                      ? FileManager::OutputPathMode::kOverwrite
                                                      : FileManager::OutputPathMode::kAppendCounter;

    FileProcessor processor;
    int processed = 0;

    for (const QString& input_path : input_paths) {
        if (cancel_requested_.loadRelaxed()) {
            emit status_message(tr("Остановлено пользователем."));
            emit finished();
            return;
        }

        QString output_path = file_manager_->get_output_path_for(
            input_path, settings_.output_directory(), path_mode);

        QFileInfo input_info(input_path);
        emit status_message(
            tr("Обработка: %1").arg(input_info.fileName()));

        bool ok = processor.process_file(
            input_path, output_path, xor_key,
            [this, &input_info](int percent) {
              emit progress_file(input_info.fileName(), percent);
            },
            [this]() {
              QCoreApplication::processEvents();
              return cancel_requested_.loadRelaxed() != 0;
            });
        if (!ok) {
          if (cancel_requested_.loadRelaxed()) {
            emit status_message(tr("Остановлено пользователем."));
          } else {
            emit error_occurred(
                tr("Ошибка обработки файла: %1").arg(input_path));
          }
          emit finished();
          return;
        }
        ++processed;
        emit progress_overall(static_cast<int>((100 * processed) / total_files));

        if (delete_input) {
          QFile::remove(input_path);
        }
  }

  emit status_message(tr("Готово. Обработано файлов: %1").arg(processed));
  emit progress_overall(100);
  emit finished();
}
