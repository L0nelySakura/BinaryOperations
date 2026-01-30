#import <QString>

struct Settings {
    QString inputMask;
    bool deleteInputFiles;
    QString outputPath;
    bool overwriteOutput;
    bool addCounter;
    bool useTimer;
    int pollIntervalMs;
    QByteArray xorKey; // 8 байт
};
