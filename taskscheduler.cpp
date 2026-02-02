#include "taskscheduler.h"
#include "worker.h"

#include <QFile>
#include <QCoreApplication>

TaskScheduler::TaskScheduler(FileManager* file_manager, QObject* parent)
    : QObject(parent),
    file_manager_(file_manager),
    run_timer_(std::make_unique<QTimer>(this)),
    scan_timer_(std::make_unique<QTimer>(this)) {

    connect(run_timer_.get(), &QTimer::timeout, this, &TaskScheduler::OnRunTimer);
    connect(scan_timer_.get(), &QTimer::timeout, this, &TaskScheduler::OnScanTimer);
}

TaskScheduler::~TaskScheduler() {
    Stop();

}

void TaskScheduler::SetSettings(const Settings& settings) {
    settings_ = settings;
}

void TaskScheduler::Start() {
    if (is_active_) {
        return;
    }

    if (!file_manager_->IsValid()) {
        emit ErrorOccurred("Не заданы входная папка или маска файлов.");
        return;
    }

    if (settings_.xor_key_8_bytes().size() != 8) {
        emit ErrorOccurred("Ключ XOR должен быть 8 байт (16 hex-символов).");
        return;
    }

    if (settings_.output_directory().isEmpty()) {
        emit ErrorOccurred("Не указана выходная папка.");
        return;
    }

    ClearQueue();
    is_active_ = true;

    emit StatusMessage("Планировщик запущен");

    if (settings_.run_mode() == Settings::RunMode::kSingle) {
        QStringList files = file_manager_->GetInputFiles();
        if (files.isEmpty()) {
            emit StatusMessage("Нет файлов для обработки.");
            emit SchedulerStopped();
            is_active_ = false;
            return;
        }

        emit StatusMessage(QString("Запущен разовый режим обработки: %1 файл(ов)").arg(files.size()));
        StartWorkerWithList(files);
    } else {
        emit StatusMessage("Запущен периодический режим");

        QStringList initial_files = file_manager_->GetInputFiles();
        if (!initial_files.isEmpty()) {
            emit StatusMessage(QString("Немедленная обработка: %1 файл(ов)").arg(initial_files.size()));
            StartWorkerWithList(initial_files);
        } else {
            emit StatusMessage("Ожидание файлов...");
        }

        StartTimersIfPeriodic();
    }

    emit SchedulerStarted();
}

void TaskScheduler::Stop() {
    if (!is_active_) {
        return;
    }

    StopTimers();
    is_active_ = false;

    if (worker_thread_ && worker_thread_->isRunning()) {
        emit StopWorkerRequested();
        emit StatusMessage("Остановка обработки...");
        worker_thread_->wait(3000);
    }

    emit SchedulerStopped();
}

bool TaskScheduler::IsRunning() const {
    return is_active_;
}

void TaskScheduler::AddFilesToQueue(const QStringList& files) {
    QMutexLocker locker(&queue_mutex_);

    int added = 0;
    for (const QString& file : files) {
        if (QFile::exists(file) && !pending_files_.contains(file)) {
            pending_files_.append(file);
            added++;
        }
    }

    if (added > 0) {
        emit StatusMessage(QString("Добавлено %1 файл(ов) в очередь").arg(added));
    }
}

void TaskScheduler::ClearQueue() {
    QMutexLocker locker(&queue_mutex_);
    pending_files_.clear();
}

void TaskScheduler::ProcessImmediately(const QStringList& files) {
    if (files.isEmpty()) {
        return;
    }

    if (worker_is_running_) {
        emit ErrorOccurred("Воркер занят, невозможно начать немедленную обработку");
        AddFilesToQueue(files);
        return;
    }

    StartWorkerWithList(files);
}

void TaskScheduler::OnRunTimer() {
    if (worker_is_running_) {
        emit StatusMessage("Воркер занят, пропускаем цикл обработки.");
        return;
    }

    QStringList to_process;
    {
        QMutexLocker locker(&queue_mutex_);
        if (pending_files_.isEmpty()) {
            return;
        }
        to_process = pending_files_;
        pending_files_.clear();
    }

    if (!to_process.isEmpty()) {
        emit StatusMessage(QString("Таймер обработки: запуск %1 файл(ов)").arg(to_process.size()));
        StartWorkerWithList(to_process);
    }
}

void TaskScheduler::OnScanTimer() {
    emit StatusMessage("Сканирование входной папки...");

    if (!file_manager_->IsValid()) {
        emit ErrorOccurred("Ошибка: не задана входная папка или маска.");
        return;
    }

    QStringList current_files = file_manager_->GetInputFiles();

    int added = 0, removed = 0;
    {
        QMutexLocker locker(&queue_mutex_);

        QStringList valid_pending;
        for (int i = 0; i < pending_files_.size(); ++i) {
            const QString& f = pending_files_.at(i);
            if (QFile::exists(f)) {
                valid_pending.append(f);
            } else {
                removed++;
            }
        }
        pending_files_ = valid_pending;

        for (int i = 0; i < current_files.size(); ++i) {
            const QString& f = current_files.at(i);
            if (!pending_files_.contains(f)) {
                pending_files_.append(f);
                added++;
            }
        }
    }

    if (added > 0) {
        emit StatusMessage(QString("Найдено новых файлов: %1").arg(added));
    }
    if (removed > 0) {
        emit StatusMessage(QString("Удалено несуществующих файлов: %1").arg(removed));
    }

    if (added > 0 || removed > 0) {
        QMutexLocker locker(&queue_mutex_);
        emit StatusMessage(QString("Всего в очереди: %1 файл(ов)").arg(pending_files_.size()));
    }
}

void TaskScheduler::StartWorkerWithList(const QStringList& files) {
    if (files.isEmpty()) {
        return;
    }

    if (worker_is_running_) {
        emit ErrorOccurred("Попытка запустить воркер, когда он уже работает");
        return;
    }

    worker_is_running_ = true;

    worker_thread_ = std::make_unique<QThread>();
    worker_ = std::make_unique<Worker>(file_manager_, settings_, nullptr);
    worker_->SetFilesToProcess(files);
    worker_->moveToThread(worker_thread_.get());

    connect(worker_thread_.get(), &QThread::started, worker_.get(), &Worker::Process);
    connect(worker_.get(), &Worker::Finished, worker_thread_.get(), &QThread::quit);
    connect(worker_.get(), &Worker::Finished, this, &TaskScheduler::OnWorkerFinished);
    connect(worker_.get(), &Worker::ProgressOverall, this, &TaskScheduler::ProgressOverall);
    connect(worker_.get(), &Worker::ProgressFile, this, &TaskScheduler::ProgressFile);
    connect(worker_.get(), &Worker::StatusMessage, this, &TaskScheduler::StatusMessage);
    connect(worker_.get(), &Worker::ErrorOccurred, this, &TaskScheduler::ErrorOccurred);
    connect(this, &TaskScheduler::StopWorkerRequested, worker_.get(), &Worker::RequestCancel, Qt::QueuedConnection);

    worker_thread_->start();
}

void TaskScheduler::StartTimersIfPeriodic() {
    if (settings_.run_mode() != Settings::RunMode::kPeriodic) {
        return;
    }

    run_timer_->setInterval(settings_.run_interval_sec() * 1000);
    run_timer_->start();

    scan_timer_->setInterval(settings_.check_files_interval_sec() * 1000);
    scan_timer_->start();

    emit StatusMessage(QString("Таймеры запущены: обработка каждые %1 сек, сканирование каждые %2 сек")
                           .arg(settings_.run_interval_sec())
                           .arg(settings_.check_files_interval_sec()));
}

void TaskScheduler::StopTimers() {
    if (run_timer_) run_timer_->stop();
    if (scan_timer_) scan_timer_->stop();
}

void TaskScheduler::OnWorkerFinished() {
    worker_is_running_ = false;

    if (worker_) {
        worker_->disconnect();
        worker_.reset();
    }

    if (worker_thread_) {
        worker_thread_->quit();
        worker_thread_->wait();
        worker_thread_.reset();
    }

    if (settings_.run_mode() == Settings::RunMode::kPeriodic && is_active_) {
        emit StatusMessage("Ожидание следующего цикла...");
    } else if (settings_.run_mode() == Settings::RunMode::kSingle) {
        is_active_ = false;
        emit SchedulerStopped();
    }
}
