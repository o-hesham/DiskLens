#include "MainWindow.h"
#include "AppVersion.h"
#include "../core/jobs/ScanJob.h"
#include "../ui/models/FileTableModel.h"
#include "../ui/models/StorageTreeModel.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QSplitter>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QFileDialog>
#include <QDir>
#include <QThread>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLocale>
#include <QLineEdit>
#include <QTimer>
#include <QTabWidget>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 %2").arg(QString::fromUtf8(DiskLens::kAppName), QString::fromUtf8(DiskLens::kAppVersion)));
    resize(1100, 700);

    setupUi();
}

MainWindow::~MainWindow()
{
    if (cancellationToken_)
        cancellationToken_->requestCancellation();

    if (scanThread_ && scanThread_->isRunning())
    {
        scanThread_->quit();
        scanThread_->wait();
    }
}

void MainWindow::setupUi()
{

    // Menu Bar
    fileMenu_ = menuBar()->addMenu(QStringLiteral("&File"));
    scanMenu_ = menuBar()->addMenu(QStringLiteral("&Scan"));
    viewMenu_ = menuBar()->addMenu(QStringLiteral("&View"));
    helpMenu_ = menuBar()->addMenu(QStringLiteral("&Help"));

    // Tool Bar
    mainToolBar_ = addToolBar(QStringLiteral("Main Toolbar"));
    selectFolderAction_ = mainToolBar_->addAction(QStringLiteral("Select Folder"));
    startScanAction_ = mainToolBar_->addAction(QStringLiteral("Start Scan"));
    cancelScanAction_ = mainToolBar_->addAction(QStringLiteral("Cancel Scan"));

    fileMenu_->addAction(selectFolderAction_);
    scanMenu_->addAction(startScanAction_);
    scanMenu_->addAction(cancelScanAction_);

    startScanAction_->setEnabled(false);
    cancelScanAction_->setEnabled(false);

    auto *centralWidget = new QWidget(this);

    mainSplitter_ = new QSplitter(Qt::Horizontal, centralWidget);

    locationsPanel_ = new QWidget(mainSplitter_);
    contentPanel_ = new QWidget(mainSplitter_);
    detailsPanel_ = new QWidget(mainSplitter_);

    mainSplitter_->addWidget(locationsPanel_);
    mainSplitter_->addWidget(contentPanel_);
    mainSplitter_->addWidget(detailsPanel_);

    mainSplitter_->setStretchFactor(0, 1);
    mainSplitter_->setStretchFactor(1, 3);
    mainSplitter_->setStretchFactor(2, 1);
    mainSplitter_->setSizes({220, 660, 260});

    // Left panel
    auto *locationsLayout = new QVBoxLayout(locationsPanel_);
    locationsList_ = new QListWidget(locationsPanel_);
    locationsLayout->addWidget(locationsList_);

    // Center panel
    auto *contentLayout = new QVBoxLayout(contentPanel_);

    emptyStateLabel_ = new QLabel(QStringLiteral("Choose a folder to begin"), contentPanel_);
    emptyStateLabel_->setAlignment(Qt::AlignCenter);

    contentLayout->addWidget(emptyStateLabel_);

    searchEdit_ = new QLineEdit(contentPanel_);
    searchEdit_->setPlaceholderText(QStringLiteral("Search scanned entries..."));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setEnabled(false);

    contentLayout->addWidget(searchEdit_);

    searchTimer_ = new QTimer(this);
    searchTimer_->setSingleShot(true);
    searchTimer_->setInterval(200);

    fileTableModel_ = new DiskLens::FileTableModel(this);
    fileTableView_ = new QTableView(contentPanel_);

    fileProxyModel_ = new QSortFilterProxyModel(this);
    fileProxyModel_->setSourceModel(fileTableModel_);
    fileProxyModel_->setSortRole(DiskLens::FileTableModel::SortRole);
    fileProxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);
    fileProxyModel_->setFilterKeyColumn(-1);
    fileProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    fileTableView_->setModel(fileProxyModel_);
    fileTableView_->setSortingEnabled(true);
    fileTableView_->sortByColumn(DiskLens::FileTableModel::NameColumn, Qt::AscendingOrder);
    fileTableView_->setAlternatingRowColors(true);
    fileTableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    fileTableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fileTableView_->verticalHeader()->hide();

    contentTabs_ = new QTabWidget(contentPanel_);
    contentTabs_->addTab(fileTableView_, QStringLiteral("Files"));

    storageTreeView_ = new QTreeView(contentTabs_);
    contentTabs_->addTab(storageTreeView_, QStringLiteral("Storage Tree"));

    storageTreeModel_ = new DiskLens::StorageTreeModel(this);
    storageTreeView_->setModel(storageTreeModel_);
    storageTreeView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    storageTreeView_->setSelectionMode(QAbstractItemView::SingleSelection);
    storageTreeView_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    contentTabs_->hide();
    contentLayout->addWidget(contentTabs_);

    // Right panel
    auto *detailsLayout = new QVBoxLayout(detailsPanel_);
    detailsLabel_ = new QLabel(QStringLiteral("No file selected"), detailsPanel_);
    detailsLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailsLabel_->setWordWrap(true);
    detailsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailsLayout->addWidget(detailsLabel_);

    // Main window content
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(mainSplitter_);

    setCentralWidget(centralWidget);

    // Status bar
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusLabel_ = new QLabel(QStringLiteral("Ready"), statusBar_);
    statusBar_->addWidget(statusLabel_, 1);

    connect(selectFolderAction_, &QAction::triggered, this, [this]()
            {
        const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Select folder"));
        if (folder.isEmpty())
            return;

        const QString cleanFolder = QDir::cleanPath(folder);
        if (!locationsList_->findItems(cleanFolder, Qt::MatchExactly).isEmpty()) // Return empty if no dupe
            return;

        locationsList_->addItem(cleanFolder);
        locationsList_->setCurrentRow(locationsList_->count() - 1);

        startScanAction_->setEnabled(true);
        statusLabel_->setText(QStringLiteral("Folder added")); });

    connect(startScanAction_, &QAction::triggered, this, &MainWindow::startScan);
    connect(cancelScanAction_, &QAction::triggered, this, &MainWindow::cancelScan);
    connect(fileTableView_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::updateDetails);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this]()
            { searchTimer_->start(); });
    connect(searchTimer_, &QTimer::timeout, this, [this]()
            { fileProxyModel_->setFilterFixedString(searchEdit_->text()); });
    connect(storageTreeView_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::updateTreeDetails);
    connect(contentTabs_, &QTabWidget::currentChanged, this, [this](int tabIndex)
            {
        if(contentTabs_->widget(tabIndex) == fileTableView_)
            updateDetails(fileTableView_->currentIndex());
        else
            updateTreeDetails(storageTreeView_->currentIndex()); });
}

void MainWindow::startScan()
{
    if (scanThread_)
        return;

    const auto *selectedItem = locationsList_->currentItem();
    if (!selectedItem)
    {
        statusLabel_->setText(QStringLiteral("Select a folder first"));
        return;
    }

    cancellationToken_ = std::make_shared<DiskLens::CancellationToken>();

    scanThread_ = new QThread(this);
    scanJob_ = new DiskLens::ScanJob(selectedItem->text(), cancellationToken_);

    // Move the scan job to the background thread so scanning does not freeze the UI.
    scanJob_->moveToThread(scanThread_);

    startScanAction_->setEnabled(false);
    cancelScanAction_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("Scanning..."));

    fileTableModel_->setEntries({});
    contentTabs_->hide();

    storageTreeModel_->setRoot(nullptr);
    storageTree_.build({});

    emptyStateLabel_->setText(QStringLiteral("Scanning..."));
    emptyStateLabel_->show();

    searchEdit_->clear();
    searchEdit_->setEnabled(false);

    detailsLabel_->setText(QStringLiteral("No file selected"));

    // Start scanning when the background thread begins running.
    connect(scanThread_, &QThread::started, scanJob_, &DiskLens::ScanJob::run);

    // Send the completed scan result back to MainWindow for UI updates.
    connect(scanJob_, &DiskLens::ScanJob::finished, this, &MainWindow::handleScanFinished);

    // Stop the background thread after the scan finishes.
    connect(scanJob_, &DiskLens::ScanJob::finished, scanThread_, &QThread::quit);

    // Safely delete the scan job in its own thread.
    connect(scanJob_, &DiskLens::ScanJob::finished, scanJob_, &QObject::deleteLater);

    // Clear references after the thread is deleted to prevent dangling pointers.
    connect(scanThread_, &QObject::destroyed, this, [this]()
            {
        scanThread_ = nullptr;
        scanJob_ = nullptr;
        cancellationToken_.reset(); 
    
        startScanAction_->setEnabled(locationsList_->count() > 0); });

    // Safely delete the thread object after it stops.
    connect(scanThread_, &QThread::finished, scanThread_, &QObject::deleteLater);

    scanThread_->start();
}

void MainWindow::cancelScan()
{
    if (!cancellationToken_)
        return;

    cancellationToken_->requestCancellation();

    cancelScanAction_->setEnabled(false);
    statusLabel_->setText(QStringLiteral("Cancelling..."));
}

void MainWindow::handleScanFinished(const DiskLens::ScanResult &result)
{
    cancelScanAction_->setEnabled(false);

    storageTreeModel_->setRoot(nullptr);

    if (result.status == DiskLens::ScanStatus::InvalidRoot)
        storageTree_.build({});
    else
        storageTree_.build(result);

    storageTreeModel_->setRoot(storageTree_.root());
    fileTableModel_->setEntries(result.entries);

    searchEdit_->setEnabled(!result.entries.isEmpty());

    if (result.entries.isEmpty())
    {
        contentTabs_->hide();

        emptyStateLabel_->setText(QStringLiteral("No scan results"));
        emptyStateLabel_->show();
    }
    else
    {
        emptyStateLabel_->hide();
        contentTabs_->show();
    }

    switch (result.status)
    {
    case DiskLens::ScanStatus::Completed:
    {
        const auto *root = storageTree_.root();
        if (root)
            statusLabel_->setText(
                QStringLiteral("Scan complete: %1 files, %2 bytes").arg(root->fileCount).arg(root->totalSizeBytes));
        break;
    }

    case DiskLens::ScanStatus::Cancelled:
        statusLabel_->setText(
            QStringLiteral("Scan cancelled"));
        break;

    case DiskLens::ScanStatus::InvalidRoot:
        statusLabel_->setText(
            QStringLiteral("The selected folder is invalid"));
        break;
    }
}

void MainWindow::updateDetails(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
    {
        detailsLabel_->setText(QStringLiteral("No file selected"));
        return;
    }

    // The table uses the proxy model for sorting.
    // Convert its row index back to the original FileTableModel index.
    const QModelIndex sourceIndex = fileProxyModel_->mapToSource(proxyIndex);

    // Retrieve the FileEntry represented by the selected source row.
    const DiskLens::FileEntry *entry = fileTableModel_->entryAt(sourceIndex.row());
    if (!entry)
    {
        detailsLabel_->setText(QStringLiteral("No file selected"));
        return;
    }

    QString kind;
    if (entry->isSymbolicLink)
        kind = QStringLiteral("Symbolic link");
    else if (entry->isDirectory)
        kind = QStringLiteral("Folder");
    else
        kind = QStringLiteral("File");

    const QString size = entry->isDirectory ? QStringLiteral("—") : QLocale().formattedDataSize(entry->sizeBytes);

    QString details = QStringLiteral("Name: ");
    details += entry->name;
    details += QStringLiteral("\nKind: ");
    details += kind;
    details += QStringLiteral("\nSize: ");
    details += size;
    details += QStringLiteral("\nModified: ");
    details += QLocale().toString(
        entry->lastModified, QLocale::ShortFormat);
    details += QStringLiteral("\n\nPath:\n");
    details += entry->absolutePath;

    detailsLabel_->setText(details);
}

void MainWindow::updateTreeDetails(const QModelIndex &index)
{
    const DiskLens::StorageNode *node = storageTreeModel_->nodeAt(index);
    if (!node)
    {
        detailsLabel_->setText(QStringLiteral("No item selected"));
        return;
    }

    const QString size = node->entry.isSymbolicLink ? QStringLiteral("—") : QLocale().formattedDataSize(node->totalSizeBytes);

    QString details = QStringLiteral("Name: %1\nSize: %2").arg(node->entry.name, size);

    if (node->entry.isDirectory && !node->entry.isSymbolicLink)
    {
        details += QStringLiteral("\nFiles: %1").arg(QLocale().toString(static_cast<qlonglong>(node->fileCount)));
    }

    details += QStringLiteral("\n\nPath:\n%1").arg(node->entry.absolutePath);

    detailsLabel_->setText(details);
}
