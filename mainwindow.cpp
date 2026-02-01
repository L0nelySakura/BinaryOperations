#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "worker.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>

namespace {
constexpr int kXorKeyBytes = 8;
constexpr int kHexCharsPerByte = 2;
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    file_manager_(new FileManager(this)),
    run_timer_(nullptr),
    scan_timer_(nullptr),
    worker_(nullptr),
    worker_thread_(nullptr),
    has_active_settings_(false) {
    ui->setupUi(this);

    connect(file_manager_.data(), &FileManager::error_occurred, ui->logTextEdit,
            &QPlainTextEdit::appendPlainText);

    connect(ui->inputMaskEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        settings_.set_input_file_mask(text.trimmed());
        file_manager_->set_file_mask(text.trimmed());
    });

    connect(ui->browseInputButton, &QPushButton::clicked, this,
            &MainWindow::on_browse_input_directory);
    connect(ui->browseOutputButton, &QPushButton::clicked, this,
            &MainWindow::on_browse_output_directory);

    connect(ui->startStopButton, &QPushButton::clicked, this,
            &MainWindow::on_start_stop_clicked);
    connect(ui->clearLogButton, &QPushButton::clicked, ui->logTextEdit,
            &QPlainTextEdit::clear);

    connect_ui_to_settings();
}

MainWindow::~MainWindow() {
    stop_timers();

    if (worker_thread_) {
        if (worker_thread_->isRunning()) {
            emit stop_requested();
            worker_thread_->quit();
            worker_thread_->wait();
        }
        worker_thread_->deleteLater();
        worker_thread_ = nullptr;
    }

    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }

    delete ui;
}

void MainWindow::connect_ui_to_settings() {
    connect(ui->deleteInputCheckBox, &QCheckBox::toggled, this,
            [this](bool checked) { settings_.set_delete_input_files(checked); });

    connect(ui->overwriteOutputRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_output_name_conflict(Settings::OutputNameConflict::kOverwrite);
                }
            });
    connect(ui->appendCounterRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_output_name_conflict(Settings::OutputNameConflict::kAppendCounter);
                }
            });

    connect(ui->singleRunRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) settings_.set_run_mode(Settings::RunMode::kSingle);
            });
    connect(ui->periodicRunRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) settings_.set_run_mode(Settings::RunMode::kPeriodic);
            });

    connect(ui->runIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) { settings_.set_run_interval_sec(value); });

    connect(ui->checkFilesIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) { settings_.set_check_files_interval_sec(value); });

    connect(ui->inputKeyEdit, &QLineEdit::textChanged, this,
            [this](const QString& text) {
                settings_.set_xor_key_8_bytes(parse_hex_to_8_bytes(text.trimmed()));
            });

    // Инициализация UI -> settings
    settings_.set_delete_input_files(ui->deleteInputCheckBox->isChecked());
    settings_.set_output_name_conflict(
        ui->overwriteOutputRadioButton->isChecked()
            ? Settings::OutputNameConflict::kOverwrite
            : Settings::OutputNameConflict::kAppendCounter);
    settings_.set_run_mode(ui->singleRunRadioButton->isChecked()
                               ? Settings::RunMode::kSingle
                               : Settings::RunMode::kPeriodic);
    settings_.set_run_interval_sec(ui->runIntervalSpinBox->value());
    settings_.set_check_files_interval_sec(ui->checkFilesIntervalSpinBox->value());
    settings_.set_input_file_mask(ui->inputMaskEdit->text().trimmed());
    settings_.set_xor_key_8_bytes(parse_hex_to_8_bytes(ui->inputKeyEdit->text().trimmed()));
}

QByteArray MainWindow::parse_hex_to_8_bytes(const QString& hex_string) {
    QString hex = hex_string;
    hex.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    const int needed_len = kXorKeyBytes * kHexCharsPerByte;
    if (hex.size() < needed_len) return QByteArray();
    if (hex.size() > needed_len) hex = hex.left(needed_len);
    return QByteArray::fromHex(hex.toLatin1());
}

void MainWindow::on_browse_input_directory() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Выбор исходной папки"));
    if (dir.isEmpty()) return;

    ui->inputPathLabel->setText(dir);
    settings_.set_input_directory(dir);
    file_manager_->set_input_directory(dir);
}

void MainWindow::on_browse_output_directory() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Выбор выходной папки"));
    if (dir.isEmpty()) return;

    ui->outputPathLabel->setText(dir);
    settings_.set_output_directory(dir);
    file_manager_->set_output_directory(dir);
}

void MainWindow::on_start_stop_clicked() {
    const bool worker_running = worker_thread_ && worker_thread_->isRunning();
    const bool timers_active = (run_timer_ && run_timer_->isActive()) ||
                               (scan_timer_ && scan_timer_->isActive());

    if (worker_running || timers_active) {
        stop_timers();
        has_active_settings_ = false;
        if (worker_running) emit stop_requested();
        ui->logTextEdit->appendPlainText(tr("Остановка."));
        ui->startStopButton->setText(tr("Старт"));
        ui->statusStatusLabel->setText(tr("Готово"));
        ui->currentFileProgressBar->setValue(0);
        ui->currentFileStatusLabel->clear();
        if (worker_running) ui->overallProgressBar->setValue(0);
        return;
    }

    // Валидация
    if (!file_manager_->is_valid()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не заданы входная папка или маска файлов."));
        return;
    }
    if (settings_.xor_key_8_bytes().size() != 8) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Ключ XOR должен быть 8 байт (16 hex-символов)."));
        return;
    }
    if (settings_.output_directory().isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не указана выходная папка."));
        return;
    }

    pending_files_.clear();

    if (!has_active_settings_) {
        active_settings_ = settings_;
        has_active_settings_ = true;
    }

    if (settings_.run_mode() == Settings::RunMode::kSingle) {
        QStringList files = file_manager_->input_files();
        if (files.isEmpty()) {
            QMessageBox::information(this, tr("Информация"), tr("Нет файлов для обработки."));
            return;
        }
        start_worker_with_list(files);
        ui->logTextEdit->appendPlainText(tr("Запущен разовый режим обработки."));
    } else {
        ui->startStopButton->setText(tr("Стоп"));
        ui->statusStatusLabel->setText(tr("Запущен режим по таймеру"));

        QStringList initial_files = file_manager_->input_files();
        if (!initial_files.isEmpty()) {
            ui->logTextEdit->appendPlainText(
                tr("Немедленная обработка: %1 файл(ов)").arg(initial_files.size()));
            start_worker_with_list(initial_files);
        } else {
            ui->statusStatusLabel->setText(tr("Ожидание файлов..."));
        }

        start_timers_if_periodic();
    }
}

void MainWindow::start_worker_with_list(const QStringList& files) {
    if (files.isEmpty()) return;

    worker_thread_ = new QThread(this);
    worker_ = new Worker(file_manager_.data(), active_settings_, nullptr);
    worker_->set_files_to_process(files);
    worker_->moveToThread(worker_thread_);

    connect(worker_thread_, &QThread::started, worker_, &Worker::process);
    connect(worker_, &Worker::finished, worker_thread_, &QThread::quit);
    connect(worker_, &Worker::finished, this, &MainWindow::on_worker_finished);
    connect(worker_, &Worker::progress_overall, this, &MainWindow::on_worker_progress_overall);
    connect(worker_, &Worker::progress_file, this, &MainWindow::on_worker_progress_file);
    connect(worker_, &Worker::status_message, this, &MainWindow::on_worker_status_message);
    connect(worker_, &Worker::error_occurred, this, &MainWindow::on_worker_error);
    connect(this, &MainWindow::stop_requested, worker_, &Worker::request_cancel, Qt::QueuedConnection);

    ui->startStopButton->setText(tr("Стоп"));
    ui->currentFileProgressBar->setValue(0);
    ui->overallProgressBar->setValue(0);
    ui->currentFileStatusLabel->clear();
    ui->statusStatusLabel->setText(tr("Обработка..."));

    worker_thread_->start();
}

void MainWindow::start_timers_if_periodic() {
    if (!run_timer_) {
        run_timer_ = new QTimer(this);
        connect(run_timer_, &QTimer::timeout, this, &MainWindow::on_run_timer);
    }
    run_timer_->setInterval(active_settings_.run_interval_sec() * 1000);
    run_timer_->start();

    if (!scan_timer_) {
        scan_timer_ = new QTimer(this);
        connect(scan_timer_, &QTimer::timeout, this, &MainWindow::on_scan_timer);
    }
    scan_timer_->setInterval(active_settings_.check_files_interval_sec() * 1000);
    scan_timer_->start();
}

void MainWindow::stop_timers() {
    if (run_timer_) run_timer_->stop();
    if (scan_timer_) scan_timer_->stop();
}

void MainWindow::on_run_timer() {
    if (worker_thread_ && worker_thread_->isRunning()) {
        ui->logTextEdit->appendPlainText(tr("Воркер занят, пропускаем цикл обработки."));
        return;
    }
    if (pending_files_.isEmpty()) {
        ui->logTextEdit->appendPlainText(tr("Очередь пуста, обработка не требуется."));
        return;
    }

    QStringList to_process = pending_files_;
    pending_files_.clear();

    ui->logTextEdit->appendPlainText(
        tr("Таймер обработки: запуск %1 файл(ов)").arg(to_process.size()));

    start_worker_with_list(to_process);
}

void MainWindow::on_scan_timer() {
    ui->logTextEdit->appendPlainText(tr("Таймер сканирования: проверка папки."));

    if (!file_manager_->is_valid()) {
        ui->logTextEdit->appendPlainText(tr("Ошибка: не задана входная папка или маска."));
        return;
    }

    QStringList current_files = file_manager_->input_files();
    int added = 0, removed = 0;

    QStringList valid_pending;
    for (const QString& f : pending_files_) {
        if (QFile::exists(f)) valid_pending.append(f);
        else removed++;
    }
    pending_files_ = valid_pending;

    for (const QString& f : current_files) {
        if (!pending_files_.contains(f)) {
            pending_files_.append(f);
            added++;
        }
    }

    if (added) ui->logTextEdit->appendPlainText(tr("Добавлено новых файлов: %1").arg(added));
    if (removed) ui->logTextEdit->appendPlainText(tr("Удалено файлов: %1").arg(removed));
    if (added || removed)
        ui->logTextEdit->appendPlainText(tr("Всего в очереди: %1").arg(pending_files_.size()));
}

void MainWindow::on_worker_finished() {
    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }
    if (worker_thread_) {
        worker_thread_->quit();
        worker_thread_->wait();
        worker_thread_->deleteLater();
        worker_thread_ = nullptr;
    }

    ui->currentFileProgressBar->setValue(0);
    ui->currentFileStatusLabel->clear();

    if (settings_.run_mode() == Settings::RunMode::kSingle) {
        ui->startStopButton->setText(tr("Старт"));
        ui->statusStatusLabel->setText(tr("Готово"));
    } else {
        bool timers_active = (run_timer_ && run_timer_->isActive()) ||
                             (scan_timer_ && scan_timer_->isActive());
        if (timers_active)
            ui->statusStatusLabel->setText(tr("Ожидание следующего цикла..."));
        else {
            ui->startStopButton->setText(tr("Старт"));
            ui->statusStatusLabel->setText(tr("Готово"));
        }
    }
}

void MainWindow::on_worker_progress_overall(int percent) {
    ui->overallProgressBar->setValue(percent);
}

void MainWindow::on_worker_progress_file(const QString& file_name, int percent) {
    ui->currentFileStatusLabel->setText(file_name);
    ui->currentFileProgressBar->setValue(percent);
}

void MainWindow::on_worker_status_message(const QString& message) {
    ui->logTextEdit->appendPlainText(message);
    ui->statusStatusLabel->setText(message);
}

void MainWindow::on_worker_error(const QString& message) {
    ui->logTextEdit->appendPlainText(message);
    ui->statusStatusLabel->setText(tr("Ошибка"));
}
