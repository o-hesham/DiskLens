#include "FilesystemScanner.h"
#include "../operations/OperationRecord.h"

#include <QDirListing>
#include <QFileInfo>

namespace DiskLens
{

    ScanResult FilesystemScanner::scanDirectory(const QString &rootPath, const CancellationToken &token) const
    {
        ScanResult result;

        const QFileInfo rootInfo{rootPath};
        result.rootPath = rootInfo.absoluteFilePath();

        if (!rootInfo.exists() || !rootInfo.isDir())
        {
            result.status = ScanStatus::InvalidRoot;
            return result;
        }

        using Flag = QDirListing::IteratorFlag;

        const auto flags = Flag::Recursive | Flag::IncludeHidden | Flag::IncludeBrokenSymlinks;

        // Scan each file and folder, then save its information.
        for (const auto &directoryEntry : QDirListing(result.rootPath, flags))
        {
            if (token.isCancellationRequested())
            {
                result.status = ScanStatus::Cancelled;
                return result;
            }

            const QFileInfo info = directoryEntry.fileInfo();

            // Skip DiskLens recovery folders and everything inside them.
            const QString path = info.absoluteFilePath();
            if (info.fileName() == kRecoveryFolderName || path.contains(QLatin1Char('/') + kRecoveryFolderName + QLatin1Char('/')))
                continue;

            FileEntry entry;
            entry.absolutePath = info.absoluteFilePath();
            entry.name = info.fileName();
            entry.sizeBytes = info.isFile() ? info.size() : 0;
            entry.lastModified = info.lastModified();
            entry.isDirectory = info.isDir();
            entry.isSymbolicLink = info.isSymLink();

            result.entries.append(entry);
        }

        return result;
    }

} // namespace DiskLens