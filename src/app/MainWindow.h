#pragma once

#include "../core/analysis/StorageTree.h"

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
class QTableView;
class QSortFilterProxyModel;
class QModelIndex;
class QLineEdit;
class QTimer;
class QTabWidget;
class QTreeView;

namespace DiskLens
{
    class ScanJob;
    class CancellationToken;
    class FileTableModel;
    class StorageTreeModel;
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

    void startScan();
    void cancelScan();
    void handleScanFinished(const DiskLens::ScanResult &result);

    void updateDetails(const QModelIndex &proxyIndex);

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

    QTabWidget *contentTabs_ = nullptr;

    QTableView *fileTableView_ = nullptr;
    DiskLens::FileTableModel *fileTableModel_ = nullptr;

    DiskLens::StorageTree storageTree_;
    DiskLens::StorageTreeModel *storageTreeModel_ = nullptr;
    QTreeView *storageTreeView_ = nullptr;

    QSortFilterProxyModel *fileProxyModel_ = nullptr;

    QLineEdit *searchEdit_ = nullptr;

    QTimer *searchTimer_ = nullptr;
};
