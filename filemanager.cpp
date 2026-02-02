#include "filemanager.h"

#include <QDir>
#include <QFileInfo>


FileManager::FileManager(QObject* parent)
    : QObject(parent)
    , input_directory_("")
    , output_directory_("")
    , file_mask_("") {
}

bool FileManager::SetInputDirectory(const QString& directory_path) {
    if (!DirectoryExists(directory_path)) {
        emit ErrorOccurred("Входная папка не существует: " + directory_path);
        input_directory_.clear();
        return false;
    }
    input_directory_ = directory_path;
    return true;
}

bool FileManager::SetOutputDirectory(const QString& directory_path) {
    if (!directory_path.isEmpty() && !DirectoryExists(directory_path)) {
        emit ErrorOccurred("Выходная папка не существует: " + directory_path);
        output_directory_.clear();
        return false;
    }
    output_directory_ = directory_path;
    return true;
}

bool FileManager::SetFileMask(const QString& file_mask) {
    if (!MaskIsValid(file_mask)) {
        emit ErrorOccurred("Некорректная маска файлов: " + file_mask);
        file_mask_.clear();
        return false;
    }
    file_mask_ = file_mask;
    return true;
}

bool FileManager::IsValid() const {
    return !input_directory_.isEmpty() && !file_mask_.isEmpty();
}

const QStringList FileManager::GetInputFiles() {
    if (!IsValid()) {
        emit ErrorOccurred(
            "Невозможно получить список файлов: параметры не заданы");
        return {};
    }

    QDir directory(input_directory_);
    QStringList files = directory.entryList({file_mask_},
                                            QDir::Files | QDir::Readable,
                                            QDir::Name);

    if (files.isEmpty()) {
        emit ErrorOccurred("Файлы по заданной маске не найдены");
    }

    for (QString& file : files) {
        file = directory.absoluteFilePath(file);
    }

    return files;
}

QString FileManager::GetOutputPathFor(const QString& input_file_path,
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

bool FileManager::DirectoryExists(const QString& path) const {
    QFileInfo info(path);
    return info.exists() && info.isDir();
}

bool FileManager::MaskIsValid(const QString& mask) const {
    if (mask.trimmed().isEmpty()) return false;
    return mask.contains('*') || mask.contains('.');
}
