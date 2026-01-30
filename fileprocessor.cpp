#include "fileprocessor.h"

#include <QFile>

void FileProcessor::xor_chunk(QByteArray& chunk, const QByteArray& key) {
    if (key.size() != 8) return;
    const char* k = key.constData();
    const int key_len = 8;
    char* data = chunk.data();
    const int size = chunk.size();
    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<char>(static_cast<uchar>(data[i]) ^
                                    static_cast<uchar>(k[i % key_len]));
    }
}

bool FileProcessor::process_file(
    const QString& input_path,
    const QString& output_path,
    const QByteArray& xor_key_8_bytes,
    std::function<void(int percent)> progress_callback) {
    if (xor_key_8_bytes.size() != 8) {
        return false;
    }

    QFile in_file(input_path);
    if (!in_file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QFile out_file(output_path);
    if (!out_file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const qint64 total_size = in_file.size();
    qint64 read_total = 0;
    int last_percent = -1;

    while (!in_file.atEnd()) {
        QByteArray chunk = in_file.read(kChunkSizeBytes);
        if (chunk.isEmpty() && !in_file.atEnd()) {
            return false;
        }
        if (chunk.isEmpty()) {
            break;
        }

        xor_chunk(chunk, xor_key_8_bytes);

        if (out_file.write(chunk) != chunk.size()) {
            return false;
        }

        read_total += chunk.size();
        if (progress_callback && total_size > 0) {
            int percent = static_cast<int>((100 * read_total) / total_size);
            if (percent != last_percent) {
                last_percent = percent;
                progress_callback(percent);
            }
        }
    }

    out_file.close();
    in_file.close();
    if (progress_callback) {
        progress_callback(100);
    }
    return true;
}
