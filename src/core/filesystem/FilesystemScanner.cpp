#include "FilesystemScanner.h"

#include <QDirListing>
#include <QFileInfo>

namespace DiskLens
{

    ScanResult FilesystemScanner::scanDirectory(const QString &rootPath, const CancellationToken &token) const
    {
        ScanResult result;

        const QFileInfo rootInfo{rootPath};
        if (!rootInfo.exists() || !rootInfo.isDir())
        {
            result.status = ScanStatus::InvalidRoot;
            return result;
        }

        using Flag = QDirListing::IteratorFlag;

        const auto flags = Flag::Recursive | Flag::IncludeHidden | Flag::IncludeBrokenSymlinks;

        // Scan each file and folder, then save its information.
        for (const auto &directoryEntry : QDirListing(rootPath, flags))
        {
            if (token.isCancellationRequested())
            {
                result.status = ScanStatus::Cancelled;
                return result;
            }

            const QFileInfo info = directoryEntry.fileInfo();

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