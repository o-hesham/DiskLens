#pragma once

#include "FileEntry.h"

#include <QVector>
#include <QMetaType>
#include <QString>

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
        QString rootPath;
        QVector<FileEntry> entries;
        ScanStatus status = ScanStatus::Completed;
    };

} // namespace DiskLens

Q_DECLARE_METATYPE(DiskLens::ScanResult)
