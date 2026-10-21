#include "MainWindow.h"
#include "AppVersion.h"
#include "../core/jobs/ScanJob.h"
#include "../core/jobs/DuplicateJob.h"
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
#include <QStringList>
#include <QFileInfo>
#include <QStandardItemModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 %2").arg(QString::fromUtf8(DiskLens::kAppName), QString::fromUtf8(DiskLens::kAppVersion)));
    resize(1100, 700);

    qRegisterMetaType<DiskLens::DuplicateProgress>();
    qRegisterMetaType<DiskLens::DuplicateResult>();

    setupUi();
}

MainWindow::~MainWindow()
{
    if (cancellationToken_)
        cancellationToken_->requestCancellation();

    if (duplicateCancellationToken_)
        duplicateCancellationToken_->requestCancellation();

    if (scanThread_ && scanThread_->isRunning())
    {
        scanThread_->quit();
        scanThread_->wait();
    }

    if (duplicateThread_)
    {
        duplicateThread_->quit();
        duplicateThread_->wait();
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
    findDuplicatesAction_ = mainToolBar_->addAction(QStringLiteral("Find Duplicates"));

    fileMenu_->addAction(selectFolderAction_);
    scanMenu_->addAction(startScanAction_);
    scanMenu_->addAction(cancelScanAction_);
    scanMenu_->addAction(findDuplicatesAction_);

    startScanAction_->setEnabled(false);
    cancelScanAction_->setEnabled(false);
    findDuplicatesAction_->setEnabled(false);

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
    storageTreeProxyModel_ = new QSortFilterProxyModel(this);
    storageTreeProxyModel_->setSourceModel(storageTreeModel_);
    storageTreeProxyModel_->setSortRole(DiskLens::StorageTreeModel::SortRole);
    storageTreeProxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);

    storageTreeView_->setModel(storageTreeProxyModel_);
    storageTreeView_->setSortingEnabled(true);
    storageTreeView_->sortByColumn(DiskLens::StorageTreeModel::SizeColumn, Qt::DescendingOrder);

    storageTreeView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    storageTreeView_->setSelectionMode(QAbstractItemView::SingleSelection);
    storageTreeView_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    duplicatesModel_ = new QStandardItemModel(this);
    duplicatesModel_->setHorizontalHeaderLabels({QStringLiteral("Group / File"), QStringLiteral("Size"), QStringLiteral("Path")});

    duplicatesProxyModel_ = new QSortFilterProxyModel(this);
    duplicatesProxyModel_->setSourceModel(duplicatesModel_);
    duplicatesProxyModel_->setFilterKeyColumn(-1);
    duplicatesProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    duplicatesProxyModel_->setRecursiveFilteringEnabled(true);

    duplicatesTreeView_ = new QTreeView(contentTabs_);
    duplicatesTreeView_->setModel(duplicatesProxyModel_);
    duplicatesTreeView_->setAlternatingRowColors(true);
    duplicatesTreeView_->setUniformRowHeights(true);
    duplicatesTreeView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    duplicatesTreeView_->setSelectionMode(QAbstractItemView::SingleSelection);
    duplicatesTreeView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    duplicatesTreeView_->setColumnWidth(0, 240);
    duplicatesTreeView_->setColumnWidth(1, 100);

    const int duplicatesTab = contentTabs_->addTab(duplicatesTreeView_, QStringLiteral("Duplicates"));
    contentTabs_->setTabEnabled(duplicatesTab, false);

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
            {
                const QString query = searchEdit_->text();
                fileProxyModel_->setFilterFixedString(query);
                duplicatesProxyModel_->setFilterFixedString(query);

                if (!query.isEmpty())
                {
                    duplicatesTreeView_->expandAll();
                } });
    connect(storageTreeView_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::updateTreeDetails);
    connect(contentTabs_, &QTabWidget::currentChanged, this, [this](int tabIndex)
            {
            const auto *page = contentTabs_->widget(tabIndex);

        if(page == fileTableView_)
            updateDetails(fileTableView_->currentIndex());
        else if (page == storageTreeView_)
            updateTreeDetails(storageTreeView_->currentIndex());
        else if (page == duplicatesTreeView_)
            updateDuplicateDetails(duplicatesTreeView_->currentIndex());
        else
                detailsLabel_->setText(QStringLiteral("No file selected")); });
    connect(findDuplicatesAction_, &QAction::triggered, this, &MainWindow::startDuplicateAnalysis);
    connect(duplicatesTreeView_->selectionModel(), QItemSelectionModel::currentRowChanged, this, &MainWindow::updateDuplicateDetails);
}

void MainWindow::startScan()
{
    if (scanThread_ || duplicateThread_)
        return;

    const auto *selectedItem = locationsList_->currentItem();
    if (!selectedItem)
    {
        statusLabel_->setText(QStringLiteral("Select a folder first"));
        return;
    }

    findDuplicatesAction_->setEnabled(false);
    hasCompletedScan_ = false;
    scanEntries_.clear();
    duplicateResult_ = {};

    contentTabs_->setCurrentWidget(fileTableView_);
    contentTabs_->setTabEnabled(contentTabs_->indexOf(duplicatesTreeView_), false);
    duplicatesModel_->removeRows(0, duplicatesModel_->rowCount());

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

        startScanAction_->setEnabled(locationsList_->count() > 0);
        findDuplicatesAction_->setEnabled(hasCompletedScan_ && !scanEntries_.isEmpty() && !duplicateThread_); });

    // Safely delete the thread object after it stops.
    connect(scanThread_, &QThread::finished, scanThread_, &QObject::deleteLater);

    scanThread_->start();
}

void MainWindow::cancelScan()
{
    const auto token = duplicateThread_ ? duplicateCancellationToken_ : cancellationToken_;
    if (!token)
        return;

    token->requestCancellation();
    cancelScanAction_->setEnabled(false);
    statusLabel_->setText(QStringLiteral("Cancelling..."));
}

void MainWindow::handleScanFinished(const DiskLens::ScanResult &result)
{
    hasCompletedScan_ = result.status == DiskLens::ScanStatus::Completed;

    scanEntries_ = hasCompletedScan_ ? result.entries : QVector<DiskLens::FileEntry>{};

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
    const QModelIndex sourceIndex = storageTreeProxyModel_->mapToSource(index);
    const DiskLens::StorageNode *node = storageTreeModel_->nodeAt(sourceIndex);
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

void MainWindow::startDuplicateAnalysis()
{
    if (scanThread_ || duplicateThread_ || !hasCompletedScan_ || scanEntries_.isEmpty())
        return;

    duplicateCancellationToken_ = std::make_shared<DiskLens::CancellationToken>();

    duplicateThread_ = new QThread(this);
    duplicateJob_ = new DiskLens::DuplicateJob(scanEntries_, duplicateCancellationToken_);
    duplicateJob_->moveToThread(duplicateThread_);

    findDuplicatesAction_->setEnabled(false);
    startScanAction_->setEnabled(false);
    selectFolderAction_->setEnabled(false);
    cancelScanAction_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("Analyzing duplicates..."));

    connect(duplicateThread_, &QThread::started, duplicateJob_, &DiskLens::DuplicateJob::run);
    connect(duplicateJob_, &DiskLens::DuplicateJob::progressChanged, this, &MainWindow::handleDuplicateProgress, Qt::QueuedConnection);
    connect(duplicateJob_, &DiskLens::DuplicateJob::finished, this, &MainWindow::handleDuplicateFinished, Qt::QueuedConnection);
    connect(duplicateJob_, &DiskLens::DuplicateJob::finished, duplicateThread_, &QThread::quit, Qt::DirectConnection);
    connect(duplicateThread_, &QThread::finished, duplicateJob_, &QObject::deleteLater);
    connect(duplicateThread_, &QObject::destroyed, this, [this]()
            {
        duplicateThread_ = nullptr;
        duplicateJob_ = nullptr;
        duplicateCancellationToken_.reset();

        selectFolderAction_->setEnabled(true);
        startScanAction_->setEnabled(
            !scanThread_ && locationsList_->count() > 0);
        findDuplicatesAction_->setEnabled(
            !scanThread_ && hasCompletedScan_ && !scanEntries_.isEmpty()); });
    connect(duplicateThread_, &QThread::finished, duplicateThread_, &QObject::deleteLater);

    duplicateThread_->start();
}

void MainWindow::handleDuplicateProgress(const DiskLens::DuplicateProgress &progress)
{
    if (duplicateCancellationToken_ && duplicateCancellationToken_->isCancellationRequested())
        return;

    static const QStringList phases{
        QStringLiteral("Grouping by size"),
        QStringLiteral("Hashing samples"),
        QStringLiteral("Hashing full files"),
        QStringLiteral("Comparing contents"),
        QStringLiteral("Checking hard links")};

    const QString phase = phases.value(static_cast<int>(progress.phase));

    QString message =
        progress.totalOperations >= 0
            ? QStringLiteral("%1: %2/%3").arg(phase).arg(progress.completedOperations).arg(progress.totalOperations)
            : QStringLiteral("%1: %2 operations").arg(phase).arg(progress.completedOperations);

    if (!progress.currentPath.isEmpty())
        message += QStringLiteral(" - %1").arg(QFileInfo(progress.currentPath).fileName());

    statusLabel_->setText(message);
}

void MainWindow::handleDuplicateFinished(const DiskLens::DuplicateResult &result)
{
    cancelScanAction_->setEnabled(false);

    if (result.status == DiskLens::DuplicateStatus::Cancelled)
    {
        statusLabel_->setText(QStringLiteral("Duplicate analysis cancelled"));

        return;
    }

    if (result.status == DiskLens::DuplicateStatus::Failed)
    {
        statusLabel_->setText(QStringLiteral("Duplicate analysis failed: %1").arg(result.errorMessage));

        return;
    }

    duplicateResult_ = result;

    duplicatesModel_->removeRows(0, duplicatesModel_->rowCount());

    for (qsizetype groupIndex = 0; groupIndex < result.groups.size(); ++groupIndex)
    {
        const auto &group = result.groups.at(groupIndex);

        auto *groupItem = new QStandardItem(QStringLiteral("Group %1 (%2 copies)").arg(groupIndex + 1).arg(group.files.size()));

        duplicatesModel_->appendRow(
            {groupItem,
             new QStandardItem(QStringLiteral("%1 each").arg(QLocale().formattedDataSize(group.sizeBytes))),
             new QStandardItem()});

        for (const auto &file : group.files)
        {
            groupItem->appendRow(
                {new QStandardItem(QFileInfo(file.absolutePath).fileName()),
                 new QStandardItem(
                     QLocale().formattedDataSize(file.sizeBytes)),
                 new QStandardItem(file.absolutePath)});
        }
    }

    if (result.groups.isEmpty())
    {
        auto *message = new QStandardItem(QStringLiteral("No duplicate files found"));
        message->setFlags(Qt::NoItemFlags);
        duplicatesModel_->appendRow(message);
    }

    contentTabs_->setTabEnabled(contentTabs_->indexOf(duplicatesTreeView_), true);
    contentTabs_->setCurrentWidget(duplicatesTreeView_);

    statusLabel_->setText(
        QStringLiteral("Analysis complete: %1 duplicate groups, %2 issues")
            .arg(result.groups.size())
            .arg(result.issues.size()));
}

void MainWindow::updateDuplicateDetails(const QModelIndex &index)
{
    if (!index.isValid() || !index.parent().isValid())
    {
        detailsLabel_->setText(QStringLiteral("Select a file within a duplicate group"));

        return;
    }

    const QString name = index.siblingAtColumn(0).data().toString();
    const QString size = index.siblingAtColumn(1).data().toString();
    const QString path = index.siblingAtColumn(2).data().toString();

    detailsLabel_->setText(QStringLiteral("Name: %1\nSize: %2\n\nPath:\n%3").arg(name, size, path));
}
