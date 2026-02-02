#pragma once

#include <QObject>
#include <QStringList>
#include <QtGlobal>
#include <memory>

#include "fileprocessor.h"
#include "settings.h"

class FileManager;

class Worker : public QObject {
    Q_OBJECT

public:
    explicit Worker(FileManager* file_manager,
                    Settings settings,
                    QObject* parent = nullptr);
    ~Worker() override;

    void SetFilesToProcess(const QStringList& paths);
    void Process();

public slots:
    void RequestCancel();

signals:
    void ProgressOverall(int percent);
    void ProgressFile(const QString& file_name, int percent);
    void StatusMessage(const QString& message);
    void Finished();
    void ErrorOccurred(const QString& message);

private:
    FileManager* file_manager_;
    Settings settings_;
    QStringList files_to_process_;
    QAtomicInt cancel_requested_{0};
};
