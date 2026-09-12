#include "ScanJob.h"
#include "../filesystem/FilesystemScanner.h"

#include <utility>

namespace DiskLens
{

    ScanJob::ScanJob(QString rootPath, std::shared_ptr<CancellationToken> cancellationToken)
        : rootPath_(std::move(rootPath)), cancellationToken_(std::move(cancellationToken))
    {
        if (!cancellationToken_)
        {
            cancellationToken_ = std::make_shared<CancellationToken>();
        }
    }

    void ScanJob::run()
    {
        FilesystemScanner scanner;
        ScanResult result = scanner.scanDirectory(rootPath_, *cancellationToken_);

        emit finished(result);
    }

} // namespace DiskLens
