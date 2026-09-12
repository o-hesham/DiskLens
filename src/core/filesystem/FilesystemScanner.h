#pragma once

#include "ScanResult.h"
#include "../jobs/CancellationToken.h"

#include <QString>
#include <QVector>

namespace DiskLens
{

    class FilesystemScanner final
    {
    public:
        [[nodiscard]] ScanResult scanDirectory(const QString &rootPath, const CancellationToken &token) const;
    };

} // namespace DiskLens
