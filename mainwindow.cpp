#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "worker.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <utility>

namespace {

constexpr int kXorKeyBytes = 8;
constexpr int kHexCharsPerByte = 2;

}  // namespace

MainWindow::MainWindow(Settings& settings, QWidget* parent)
        : QMainWindow(parent),
        ui(new Ui::MainWindow),
        settings_(settings),
        file_manager_(new FileManager(this)) {
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

void MainWindow::connect_ui_to_settings() {
    // b) Удалять входные файлы
    connect(ui->deleteInputCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) { settings_.set_delete_input_files(checked); });

    // d) Перезапись или счётчик к имени выходного файла
    connect(ui->overwriteOutputRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_output_name_conflict(
                        Settings::OutputNameConflict::kOverwrite);
                }
            });
    connect(ui->appendCounterRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_output_name_conflict(
                        Settings::OutputNameConflict::kAppendCounter);
                }
            });

    // e) Разовый запуск или по таймеру
    connect(ui->singleRunRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_run_mode(Settings::RunMode::kSingle);
                }
            });
    connect(ui->periodicRunRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_run_mode(Settings::RunMode::kPeriodic);
                }
            });

    connect(ui->runIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) { settings_.set_run_interval_sec(value); });

    // Опрос входных файлов: по начальным или по таймеру
    connect(ui->singleCheckFilesRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_check_input_mode(
                        Settings::CheckInputMode::kInitialOnly);
                }
            });
    connect(ui->periodicCheckFilesRadioButton, &QRadioButton::toggled, this,
            [this](bool checked) {
                if (checked) {
                    settings_.set_check_input_mode(
                        Settings::CheckInputMode::kPollByTimer);
                }
            });

    connect(ui->checkFilesIntervalSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) {
                settings_.set_check_files_interval_sec(value);
            });

    // g) Ключ XOR 8 байт (ввод в hex, 16 символов)
    connect(ui->inputKeyEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            settings_.set_xor_key_8_bytes(parse_hex_to_8_bytes(text.trimmed()));
    });

    // Инициализация значений из текущего состояния UI
    settings_.set_delete_input_files(ui->deleteInputCheckBox->isChecked());
    settings_.set_output_name_conflict(
        ui->overwriteOutputRadioButton->isChecked()
            ? Settings::OutputNameConflict::kOverwrite
            : Settings::OutputNameConflict::kAppendCounter);
    settings_.set_run_mode(ui->singleRunRadioButton->isChecked()
                               ? Settings::RunMode::kSingle
                               : Settings::RunMode::kPeriodic);
    settings_.set_check_input_mode(ui->singleCheckFilesRadioButton->isChecked()
                                       ? Settings::CheckInputMode::kInitialOnly
                                       : Settings::CheckInputMode::kPollByTimer);
    settings_.set_run_interval_sec(ui->runIntervalSpinBox->value());
    settings_.set_check_files_interval_sec(ui->checkFilesIntervalSpinBox->value());
    settings_.set_input_file_mask(ui->inputMaskEdit->text().trimmed());
    settings_.set_xor_key_8_bytes(
        parse_hex_to_8_bytes(ui->inputKeyEdit->text().trimmed()));
}

QByteArray MainWindow::parse_hex_to_8_bytes(const QString& hex_string) {
    QString hex = hex_string;
    hex.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    const int needed_len = kXorKeyBytes * kHexCharsPerByte;  // 16
    if (hex.size() < needed_len) {
        return QByteArray();
    }
    if (hex.size() > needed_len) {
        hex = hex.left(needed_len);
    }
    return QByteArray::fromHex(hex.toLatin1());
}

void MainWindow::on_browse_input_directory() {
    QString directory = QFileDialog::getExistingDirectory(
        this, tr("Выбор исходной папки"));
    if (directory.isEmpty()) return;

    ui->inputPathLabel->setText(directory);
    settings_.set_input_directory(directory);
    file_manager_->set_input_directory(directory);
}

void MainWindow::on_browse_output_directory() {
    QString directory = QFileDialog::getExistingDirectory(
        this, tr("Выбор выходной папки"));
    if (directory.isEmpty()) return;

    ui->outputPathLabel->setText(directory);
    settings_.set_output_directory(directory);
    file_manager_->set_output_directory(directory);
}

void MainWindow::on_start_stop_clicked() {
    const bool worker_running =
        worker_thread_ != nullptr && worker_thread_->isRunning();
    const bool timers_active =
        (run_timer_ != nullptr && run_timer_->isActive()) ||
        (scan_timer_ != nullptr && scan_timer_->isActive());

    if (worker_running || timers_active) {
        stop_timers();
        if (worker_running) {
            emit stop_requested();
        }
        ui->logTextEdit->appendPlainText(tr("Остановка."));
        ui->startStopButton->setText(tr("Старт"));
        ui->statusStatusLabel->setText(tr("Готово"));
        ui->currentFileProgressBar->setValue(0);
        ui->currentFileStatusLabel->clear();
        if (worker_running) {
            ui->overallProgressBar->setValue(0);
        }
        return;
    }

    if (settings_.run_mode() == Settings::RunMode::kSingle) {
        QStringList files = file_manager_->input_files();
        start_worker_with_list(files);
    } else {
        pending_files_ = file_manager_->input_files();
        if (!pending_files_.isEmpty()) {
            start_worker_with_list(pending_files_);
            pending_files_.clear();
        } else {
            ui->startStopButton->setText(tr("Стоп"));
            ui->statusStatusLabel->setText(tr("Ожидание таймера..."));
        }
        start_timers_if_periodic();
        ui->logTextEdit->appendPlainText(
            tr("Таймер запуска: обработка каждые %1 сек...")
                .arg(settings_.run_interval_sec()));
        if (settings_.check_input_mode() ==
            Settings::CheckInputMode::kPollByTimer) {
            ui->logTextEdit->appendPlainText(
                tr("Таймер сканирования: проверка папки каждые %1 сек.")
                    .arg(settings_.check_files_interval_sec()));
        }
    }
}

void MainWindow::start_worker_with_list(const QStringList& files) {
    if (files.isEmpty()) {
        ui->logTextEdit->appendPlainText(tr("Нет файлов для обработки."));
        return;
    }

    worker_thread_ = new QThread(this);
    worker_ = new Worker(file_manager_.data(), settings_, nullptr);
    worker_->set_files_to_process(files);
    worker_->moveToThread(worker_thread_);

    connect(worker_thread_, &QThread::started, worker_, &Worker::process);
    connect(worker_, &Worker::finished, worker_thread_, &QThread::quit);
    connect(worker_, &Worker::finished, this, &MainWindow::on_worker_finished);
    connect(worker_, &Worker::progress_overall, this,
            &MainWindow::on_worker_progress_overall);
    connect(worker_, &Worker::progress_file, this,
            &MainWindow::on_worker_progress_file);
    connect(worker_, &Worker::status_message, this,
            &MainWindow::on_worker_status_message);
    connect(worker_, &Worker::error_occurred, this,
            &MainWindow::on_worker_error);
    connect(this, &MainWindow::stop_requested, worker_, &Worker::request_cancel,
            Qt::QueuedConnection);

    ui->startStopButton->setText(tr("Стоп"));
    ui->currentFileProgressBar->setValue(0);
    ui->overallProgressBar->setValue(0);
    ui->currentFileStatusLabel->clear();
    ui->statusStatusLabel->setText(tr("Обработка..."));
    worker_thread_->start();
}

void MainWindow::on_run_timer() {
    ui->logTextEdit->appendPlainText(tr("Таймер запуска сработал."));
    if (worker_thread_ && worker_thread_->isRunning()) {
        return;
    }
    QStringList to_process = pending_files_;
    pending_files_.clear();
    if (to_process.isEmpty()) {
        ui->logTextEdit->appendPlainText(tr("Очередь пуста, пропуск."));
        return;
    }
    start_worker_with_list(to_process);
}

void MainWindow::on_scan_timer() {
    ui->logTextEdit->appendPlainText(tr("Таймер сканирования сработал."));
    if (!file_manager_->is_valid()) return;
    QStringList current = file_manager_->input_files();
    int added = 0;
    for (const QString& path : std::as_const(current)) {
        if (!pending_files_.contains(path)) {
            pending_files_.append(path);
            ++added;
        }
    }
    if (added > 0) {
        ui->logTextEdit->appendPlainText(
            tr("Добавлено в очередь: %1 файл(ов). Всего в очереди: %2.")
                .arg(added)
                .arg(pending_files_.size()));
    }
}

void MainWindow::start_timers_if_periodic() {
    if (!run_timer_) {
        run_timer_ = new QTimer(this);
        connect(run_timer_, &QTimer::timeout, this, &MainWindow::on_run_timer);
    }
    run_timer_->setInterval(settings_.run_interval_sec() * 1000);
    run_timer_->start();

    if (settings_.check_input_mode() ==
        Settings::CheckInputMode::kPollByTimer) {
        if (!scan_timer_) {
            scan_timer_ = new QTimer(this);
            connect(scan_timer_, &QTimer::timeout, this, &MainWindow::on_scan_timer);
        }
        scan_timer_->setInterval(settings_.check_files_interval_sec() * 1000);
        scan_timer_->start();
    } else {
        if (scan_timer_) scan_timer_->stop();
    }
}

void MainWindow::stop_timers() {
    if (run_timer_) run_timer_->stop();
    if (scan_timer_) scan_timer_->stop();
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
    if (settings_.run_mode() == Settings::RunMode::kPeriodic &&
        (run_timer_ && run_timer_->isActive())) {
        ui->statusStatusLabel->setText(tr("Ожидание таймера..."));
    } else {
        ui->statusStatusLabel->setText(tr("Готово"));
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

MainWindow::~MainWindow() {
    stop_timers();
    if (worker_thread_ && worker_thread_->isRunning()) {
        emit stop_requested();
        worker_thread_->wait(3000);
    }
    delete ui;
}
