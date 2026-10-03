#pragma once

#include <QByteArray>
#include <QString>

namespace DiskLens
{

    enum class FileHashStatus
    {
        Completed,
        Cancelled,
        Failed
    };

    struct FileHashResult
    {
        FileHashStatus status = FileHashStatus::Failed;
        QByteArray digest;
        QString errorMessage;
    };

} // namespace DiskLens
