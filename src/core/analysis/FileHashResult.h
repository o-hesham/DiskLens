#pragma once

#include "DuplicateIssueKind.h"

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
        DuplicateIssueKind issueKind = DuplicateIssueKind::Other;
    };

} // namespace DiskLens
