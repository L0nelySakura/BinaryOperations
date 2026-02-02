#include "worker.h"

#include "filemanager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

Worker::Worker(FileManager* file_manager,
               Settings settings,
               QObject* parent)
    : QObject(parent),
    file_manager_(file_manager),
    settings_(std::move(settings)) {}

Worker::~Worker() {
    RequestCancel();
    QCoreApplication::processEvents();
}


void Worker::RequestCancel() {
    cancel_requested_.storeRelaxed(1);
}

void Worker::SetFilesToProcess(const QStringList& paths) {
    files_to_process_ = paths;
}

void Worker::Process() {
    if (!file_manager_->IsValid()) {
        emit ErrorOccurred("Не заданы входная папка или маска файлов.");
        emit Finished();
        return;
    }

    if (settings_.xor_key_8_bytes().size() != 8) {
        emit ErrorOccurred("Ключ XOR должен быть ровно 8 байт (16 hex-символов).");
        emit Finished();
        return;
    }

    if (settings_.output_directory().isEmpty()) {
        emit ErrorOccurred("Не указана выходная папка.");
        emit Finished();
        return;
    }

    QStringList input_paths =
        files_to_process_.isEmpty() ? file_manager_->GetInputFiles()
                                    : files_to_process_;
    if (input_paths.isEmpty()) {
        emit StatusMessage("Нет файлов для обработки.");
        emit Finished();
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
            emit StatusMessage("Остановлено пользователем.");
            emit Finished();
            return;
        }

        QString output_path = file_manager_->GetOutputPathFor(
            input_path, settings_.output_directory(), path_mode);

        QFileInfo input_info(input_path);
        emit StatusMessage(
            tr("Обработка: %1").arg(input_info.fileName()));

        bool ok = processor.ProcessFile(
            input_path, output_path, xor_key,
            [this, &input_info](int percent) {
                emit ProgressFile(input_info.fileName(), percent);
            },
            [this]() {
                QCoreApplication::processEvents();
                return cancel_requested_.loadRelaxed() != 0;
            });
        if (!ok) {
            if (cancel_requested_.loadRelaxed()) {
                emit StatusMessage("Остановлено пользователем.");
            } else {
                emit ErrorOccurred(
                    tr("Ошибка обработки файла: %1").arg(input_path));
            }
            emit Finished();
            return;
        }
        ++processed;
        emit ProgressOverall(static_cast<int>((100 * processed) / total_files));

        if (delete_input) {
            QFile::remove(input_path);
        }
    }

    emit StatusMessage(tr("Готово. Обработано файлов: %1").arg(processed));
    emit ProgressOverall(100);
    emit Finished();
}
