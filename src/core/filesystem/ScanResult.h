#pragma once

#include "FileEntry.h"

#include <QVector>
#include <QMetaType>

namespace DiskLens
{

    enum class ScanStatus
    {
        Completed,
        Cancelled,
        InvalidRoot
    };

    struct ScanResult
    {
        QVector<FileEntry> entries;
        ScanStatus status = ScanStatus::Completed;
    };

} // namespace DiskLens

Q_DECLARE_METATYPE(DiskLens::ScanResult)
