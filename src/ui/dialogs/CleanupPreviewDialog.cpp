#include "CleanupPreviewDialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace DiskLens
{

    CleanupPreviewDialog::CleanupPreviewDialog(const OperationRecord &record, QWidget *parent)
        : QDialog(parent)
    {

        setWindowTitle(QStringLiteral("Move to Recovery"));
        resize(900, 450);

        auto *table = new QTableWidget(record.items.size(), 4, this);
        table->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size"), QStringLiteral("From"), QStringLiteral("To")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);

        int movableCount = 0;
        qint64 movableBytes = 0;

        for (int row = 0; row < record.items.size(); ++row)
        {
            const auto &item = record.items.at(row);
            const bool movable = item.status == RecoveryStatus::Pending;
            if (movable)
            {
                ++movableCount;
                movableBytes += item.sizeBytes;
            }

            table->setItem(row, 0, new QTableWidgetItem(QFileInfo(item.originalPath).fileName()));
            table->setItem(row, 1, new QTableWidgetItem(QLocale().formattedDataSize(item.sizeBytes)));
            table->setItem(row, 2, new QTableWidgetItem(item.originalPath));
            table->setItem(row, 3, new QTableWidgetItem(movable ? item.recoveryPath : item.message));
        }

        auto *header = new QLabel(QStringLiteral("Move %1 files (%2) to recovery").arg(movableCount).arg(QLocale().formattedDataSize(movableBytes)), this);
        auto *note = new QLabel(QStringLiteral("Files are moved, not deleted. Restore inside DiskLens comes in a later version; until then, files can be moved back by hand from the recovery folder."), this);
        note->setWordWrap(true);

        auto *buttons = new QDialogButtonBox(this);
        auto *moveButton = buttons->addButton(QStringLiteral("Move"), (QDialogButtonBox::AcceptRole));
        auto *cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
        moveButton->setAutoDefault(false);
        moveButton->setEnabled(movableCount > 0);
        cancelButton->setDefault(true);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(header);
        layout->addWidget(table);
        layout->addWidget(note);
        layout->addWidget(buttons);
    }

} // namespace DiskLens