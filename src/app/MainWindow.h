#pragma once

#include <QMainWindow>

#include <memory>

class QMenu;
class QToolBar;
class QAction;
class QSplitter;
class QWidget;
class QListWidget;
class QLabel;
class QStatusBar;
class QThread;

namespace DiskLens
{
    class ScanJob;
    class CancellationToken;
    struct ScanResult;

} // namespace DiskLens

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();

    QMenu *fileMenu_ = nullptr;
    QMenu *scanMenu_ = nullptr;
    QMenu *viewMenu_ = nullptr;
    QMenu *helpMenu_ = nullptr;

    QToolBar *mainToolBar_ = nullptr;

    QAction *selectFolderAction_ = nullptr;
    QAction *startScanAction_ = nullptr;
    QAction *cancelScanAction_ = nullptr;

    QSplitter *mainSplitter_ = nullptr;

    QWidget *locationsPanel_ = nullptr;
    QWidget *contentPanel_ = nullptr;
    QWidget *detailsPanel_ = nullptr;

    QListWidget *locationsList_ = nullptr;
    QLabel *emptyStateLabel_ = nullptr;
    QLabel *detailsLabel_ = nullptr;

    QStatusBar *statusBar_ = nullptr;
    QLabel *statusLabel_ = nullptr;

    QThread *scanThread_ = nullptr;
    DiskLens::ScanJob *scanJob_ = nullptr;
    std::shared_ptr<DiskLens::CancellationToken> cancellationToken_;

    void startScan();
    void cancelScan();
    void handleScanFinished(const DiskLens::ScanResult &result);
};
