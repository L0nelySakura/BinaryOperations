#pragma once

#include <QByteArray>
#include <QString>

class Settings {
 public:
  // a) Маска входных файлов (.txt, testFile.bin)
  const QString& input_file_mask() const { return input_file_mask_; }
  void set_input_file_mask(const QString& value) { input_file_mask_ = value; }

  // b) Удалять входные файлы после обработки или нет
  bool delete_input_files() const { return delete_input_files_; }
  void set_delete_input_files(bool value) { delete_input_files_ = value; }

  // c) Путь для сохранения результирующих файлов
  const QString& output_directory() const { return output_directory_; }
  void set_output_directory(const QString& value) { output_directory_ = value; }

  // d) Действия при повторении имени: перезапись или счётчик к имени
  enum class OutputNameConflict { kOverwrite, kAppendCounter };
  OutputNameConflict output_name_conflict() const {
    return output_name_conflict_;
  }
  void set_output_name_conflict(OutputNameConflict value) {
    output_name_conflict_ = value;
  }

  // e) Работа по таймеру или разовый запуск
  enum class RunMode { kSingle, kPeriodic };
  RunMode run_mode() const { return run_mode_; }
  void set_run_mode(RunMode value) { run_mode_ = value; }

  // Режим опроса входных файлов: по начальным или по таймеру
  enum class CheckInputMode { kInitialOnly, kPollByTimer };
  CheckInputMode check_input_mode() const { return check_input_mode_; }
  void set_check_input_mode(CheckInputMode value) {
    check_input_mode_ = value;
  }

  // f) Периодичность опроса наличия входного файла (сек)
  int check_files_interval_sec() const { return check_files_interval_sec_; }
  void set_check_files_interval_sec(int value) {
    check_files_interval_sec_ = value;
  }

  // Интервал запуска по таймеру (сек)
  int run_interval_sec() const { return run_interval_sec_; }
  void set_run_interval_sec(int value) { run_interval_sec_ = value; }

  // g) Значение 8 байт для XOR-модификации файла
  const QByteArray& xor_key_8_bytes() const { return xor_key_8_bytes_; }
  void set_xor_key_8_bytes(const QByteArray& value) {
    xor_key_8_bytes_ = value;
  }

  // Путь к входной папке (для чтения файлов)
  const QString& input_directory() const { return input_directory_; }
  void set_input_directory(const QString& value) { input_directory_ = value; }

 private:
  QString input_directory_;
  QString input_file_mask_;
  QString output_directory_;
  QByteArray xor_key_8_bytes_;
  bool delete_input_files_ = false;
  OutputNameConflict output_name_conflict_ = OutputNameConflict::kOverwrite;
  RunMode run_mode_ = RunMode::kSingle;
  CheckInputMode check_input_mode_ = CheckInputMode::kInitialOnly;
  int run_interval_sec_ = 1;
  int check_files_interval_sec_ = 1;
};
