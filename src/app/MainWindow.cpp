#include "MainWindow.h"
#include "AppVersion.h"
#include "../core/jobs/ScanJob.h"

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

    // Right
    auto *detailsLayout = new QVBoxLayout(detailsPanel_);
    detailsLabel_ = new QLabel(QStringLiteral("No file selected"), detailsPanel_);
    detailsLabel_->setAlignment(Qt::AlignCenter);
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
                if(folder.isEmpty())
                    return;
                
                const QString cleanFolder = QDir::cleanPath(folder);
                if(!locationsList_->findItems(cleanFolder, Qt::MatchExactly).isEmpty()) //Return empty if no dupe
                    return; 

                locationsList_->addItem(cleanFolder);
                locationsList_->setCurrentRow(locationsList_->count()-1);

                startScanAction_->setEnabled(true);
                statusLabel_->setText(QStringLiteral("Folder added")); });

    connect(startScanAction_, &QAction::triggered, this, &MainWindow::startScan);
    connect(cancelScanAction_, &QAction::triggered, this, &MainWindow::cancelScan);
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

    startScanAction_->setEnabled(false);
    cancelScanAction_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("Scanning..."));

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

    switch (result.status)
    {
    case DiskLens::ScanStatus::Completed:
        statusLabel_->setText(
            QStringLiteral("Scan complete: %1 entries found").arg(result.entries.size()));
        break;

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
