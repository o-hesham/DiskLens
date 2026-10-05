#pragma once

#include "../filesystem/FileEntry.h"
#include "../jobs/CancellationToken.h"

#include <QString>

namespace DiskLens
{

    enum class FileComparisonStatus
    {
        Equal,
        Different,
        Cancelled,
        Failed
    };

    struct FileComparisonResult
    {
        FileComparisonStatus status = FileComparisonStatus::Failed;
        QString errorMessage;
    };

    class FileComparer final
    {
    public:
        [[nodiscard]] static FileComparisonResult compare(const FileEntry &first, const FileEntry &second, const CancellationToken &cancellationToken);
    };

} // namespace DiskLens
