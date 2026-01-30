#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QButtonGroup>
#include <QFileDialog>
#include <QRegularExpression>

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

MainWindow::~MainWindow() {
  delete ui;
}
