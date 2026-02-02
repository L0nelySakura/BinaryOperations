#pragma once

#include <QMainWindow>
#include <memory>

#include "filemanager.h"
#include "settings.h"
#include "taskscheduler.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void OnBrowseInputButtonClicked();
    void OnBrowseOutputButtonClicked();
    void OnStartStopButtonClicked();
    void OnClearLogButtonClicked();

private:
    void ConnectUiToSettings();
    void OnSchedulerStarted();
    void OnSchedulerStopped();
    void OnProgressOverall(int percent);
    void OnProgressFile(const QString& file_name, int percent);
    void OnStatusMessage(const QString& message);
    void OnErrorOccurred(const QString& message);

    static QByteArray ParseHexTo8Bytes(const QString& hex_string);

    std::unique_ptr<Ui::MainWindow> ui;
    Settings settings_;
    std::unique_ptr<FileManager> file_manager_;
    std::unique_ptr<TaskScheduler> scheduler_;
};
