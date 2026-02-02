#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <memory>

#include "filemanager.h"
#include "settings.h"

class Worker;

class TaskScheduler : public QObject {
    Q_OBJECT

public:
    explicit TaskScheduler(FileManager* file_manager,
                           QObject* parent = nullptr);
    ~TaskScheduler() override;

    void SetSettings(const Settings& settings);
    void Start();
    void Stop();
    bool IsRunning() const;

signals:
    void ProgressOverall(int percent);
    void ProgressFile(const QString& file_name, int percent);
    void StatusMessage(const QString& message);
    void ErrorOccurred(const QString& message);
    void SchedulerStarted();
    void SchedulerStopped();

    void StopWorkerRequested();

public slots:
    void AddFilesToQueue(const QStringList& files);
    void ClearQueue();
    void ProcessImmediately(const QStringList& files);

private slots:
    void OnWorkerFinished();
    void OnRunTimer();
    void OnScanTimer();

private:
    void StartWorkerWithList(const QStringList& files);
    void StartTimersIfPeriodic();
    void StopTimers();

    FileManager* file_manager_;
    Settings settings_;

    std::unique_ptr<Worker> worker_;
    std::unique_ptr<QThread> worker_thread_;

    std::unique_ptr<QTimer> run_timer_;
    std::unique_ptr<QTimer> scan_timer_;

    QStringList pending_files_;
    mutable QMutex queue_mutex_;

    bool is_active_ = false;
    bool worker_is_running_ = false;
};
