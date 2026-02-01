#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class FileProcessor {
 public:
  static constexpr qint64 kChunkSizeBytes = 1024 * 1024;  // 1 МБ

  FileProcessor() = default;

  // Читает файл кусками по 1 МБ, применяет XOR с ключом (8 байт), пишет в
  // output_path. progress_callback(0..100) — прогресс по текущему файлу.
  // is_cancelled() — если возвращает true, обработка прерывается (выходной
  // файл удаляется). Возвращает true при успехе.
  bool process_file(
      const QString& input_path,
      const QString& output_path,
      const QByteArray& xor_key_8_bytes,
      std::function<void(int percent)> progress_callback = nullptr,
      std::function<bool()> is_cancelled = nullptr);

 private:
  static void xor_chunk(QByteArray& chunk, const QByteArray& key);
};
