#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class FileManager : public QObject {
    Q_OBJECT

public:
    explicit FileManager(QObject* parent = nullptr);
    ~FileManager() override = default;

    bool SetInputDirectory(const QString& directory_path);
    bool SetOutputDirectory(const QString& directory_path);
    bool SetFileMask(const QString& file_mask);

    const QString& input_directory() const { return input_directory_; }
    const QString& output_directory() const { return output_directory_; }
    const QString& file_mask() const { return file_mask_; }

    bool IsValid() const;
    const QStringList GetInputFiles();

    enum class OutputPathMode { kOverwrite, kAppendCounter };
    QString GetOutputPathFor(const QString& input_file_path,
                             const QString& output_directory,
                             OutputPathMode path_mode) const;

signals:
    void ErrorOccurred(const QString& message);

private:
    bool DirectoryExists(const QString& path) const;
    bool MaskIsValid(const QString& mask) const;

    QString input_directory_;
    QString output_directory_;
    QString file_mask_;
};
