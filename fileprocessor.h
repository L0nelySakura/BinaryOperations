#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class FileProcessor {
public:
    static constexpr qint64 kChunkSizeBytes = 1024 * 1024;

    FileProcessor() = default;

    bool ProcessFile(
        const QString& input_path,
        const QString& output_path,
        const QByteArray& xor_key_8_bytes,
        std::function<void(int percent)> progress_callback = nullptr,
        std::function<bool()> is_cancelled = nullptr);

private:
    static void XorChunk(QByteArray& chunk, const QByteArray& key);
};
