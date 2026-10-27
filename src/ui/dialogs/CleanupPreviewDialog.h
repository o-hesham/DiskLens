#pragma once

#include "../../core/operations/OperationRecord.h"

#include <QDialog>

namespace DiskLens
{

    // Shows the planned recovery moves and asks the user to confirm them.
    class CleanupPreviewDialog final : public QDialog
    {
        Q_OBJECT

    public:
        explicit CleanupPreviewDialog(const OperationRecord &record, QWidget *parent = nullptr);
    };

} // namespace DiskLens
