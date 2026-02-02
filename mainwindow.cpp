#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>

namespace {
constexpr int kXorKeyBytes = 8;
constexpr int kHexCharsPerByte = 2;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>()),
    file_manager_(std::make_unique<FileManager>()),
    scheduler_(std::make_unique<TaskScheduler>(file_manager_.get(), this)) {
    ui->setupUi(this);

    connect(scheduler_.get(), &TaskScheduler::SchedulerStarted,
            this, &MainWindow::OnSchedulerStarted);
    connect(scheduler_.get(), &TaskScheduler::SchedulerStopped,
            this, &MainWindow::OnSchedulerStopped);
    connect(scheduler_.get(), &TaskScheduler::ProgressOverall,
            this, &MainWindow::OnProgressOverall);
    connect(scheduler_.get(), &TaskScheduler::ProgressFile,
            this, &MainWindow::OnProgressFile);
    connect(scheduler_.get(), &TaskScheduler::StatusMessage,
            this, &MainWindow::OnStatusMessage);
    connect(scheduler_.get(), &TaskScheduler::ErrorOccurred,
            this, &MainWindow::OnErrorOccurred);

    connect(file_manager_.get(), &FileManager::ErrorOccurred, ui->logTextEdit,
            &QPlainTextEdit::appendPlainText);

    connect(ui->inputMaskEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        settings_.set_input_file_mask(text.trimmed());
        file_manager_->SetFileMask(text.trimmed());
    });

    connect(ui->browseInputButton, &QPushButton::clicked,
            this, &MainWindow::OnBrowseInputButtonClicked);
    connect(ui->browseOutputButton, &QPushButton::clicked,
            this, &MainWindow::OnBrowseOutputButtonClicked);
    connect(ui->startStopButton, &QPushButton::clicked,
            this, &MainWindow::OnStartStopButtonClicked);
    connect(ui->clearLogButton, &QPushButton::clicked,
            this, &MainWindow::OnClearLogButtonClicked);

    ConnectUiToSettings();
}

MainWindow::~MainWindow() {
    if (scheduler_) {
        scheduler_->Stop();
    }
}

void MainWindow::ConnectUiToSettings() {
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
                settings_.set_xor_key_8_bytes(ParseHexTo8Bytes(text.trimmed()));
            });

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
    settings_.set_xor_key_8_bytes(ParseHexTo8Bytes(ui->inputKeyEdit->text().trimmed()));
}

QByteArray MainWindow::ParseHexTo8Bytes(const QString& hex_string) {
    QString hex = hex_string;
    hex.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    const int needed_len = kXorKeyBytes * kHexCharsPerByte;
    if (hex.size() < needed_len) return QByteArray();
    if (hex.size() > needed_len) hex = hex.left(needed_len);
    return QByteArray::fromHex(hex.toLatin1());
}

void MainWindow::OnBrowseInputButtonClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Выбор исходной папки"));
    if (dir.isEmpty()) return;

    ui->inputPathLabel->setText(dir);
    settings_.set_input_directory(dir);
    file_manager_->SetInputDirectory(dir);
}

void MainWindow::OnBrowseOutputButtonClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Выбор выходной папки"));
    if (dir.isEmpty()) return;

    ui->outputPathLabel->setText(dir);
    settings_.set_output_directory(dir);
    file_manager_->SetOutputDirectory(dir);
}

void MainWindow::OnStartStopButtonClicked() {
    if (scheduler_->IsRunning()) {
        scheduler_->Stop();
        ui->startStopButton->setText(tr("Старт"));
        ui->statusStatusLabel->setText(tr("Готово"));
        ui->currentFileProgressBar->setValue(0);
        ui->overallProgressBar->setValue(0);
        return;
    }

    if (!file_manager_->IsValid()) {
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

    scheduler_->SetSettings(settings_);
    scheduler_->Start();
}

void MainWindow::OnClearLogButtonClicked() {
    ui->logTextEdit->clear();
}

void MainWindow::OnSchedulerStarted() {
    ui->startStopButton->setText(tr("Стоп"));
    ui->statusStatusLabel->setText(tr("Запущен"));
    ui->currentFileProgressBar->setValue(0);
    ui->overallProgressBar->setValue(0);
}

void MainWindow::OnSchedulerStopped() {
    ui->startStopButton->setText(tr("Старт"));
    ui->statusStatusLabel->setText(tr("Готово"));
    ui->currentFileProgressBar->setValue(0);
    ui->overallProgressBar->setValue(0);
}

void MainWindow::OnProgressOverall(int percent) {
    ui->overallProgressBar->setValue(percent);
}

void MainWindow::OnProgressFile(const QString& file_name, int percent) {
    ui->currentFileStatusLabel->setText(file_name);
    ui->currentFileProgressBar->setValue(percent);
}

void MainWindow::OnStatusMessage(const QString& message) {
    ui->logTextEdit->appendPlainText(message);
    ui->statusStatusLabel->setText(message);
}

void MainWindow::OnErrorOccurred(const QString& message) {
    ui->logTextEdit->appendPlainText("ОШИБКА: " + message);
    ui->statusStatusLabel->setText(tr("Ошибка"));
}
