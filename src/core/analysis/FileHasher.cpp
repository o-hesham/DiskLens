#include "FileHasher.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace DiskLens
{

    FileHashResult FileHasher::hashSample(const FileEntry &entry, const CancellationToken &cancellationToken)
    {
        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        QFile file(entry.absolutePath);
        if (!file.open(QIODevice::ReadOnly))
            return {FileHashStatus::Failed, {}, file.errorString()};

        QFileInfo info(file);

        // Reject files whose size or modification time changed since the scan.
        if (info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {FileHashStatus::Failed, {}, QStringLiteral("File changed since the scan")};

        const qint64 size = file.size();
        constexpr qint64 chunkSize = 64 * 1024;

        if (size <= 0)
            return {FileHashStatus::Failed, {}, QStringLiteral("Expected a non-empty file")};

        const qint64 firstReadSize = size <= 2 * chunkSize ? size : chunkSize;

        QByteArray contents = file.read(firstReadSize);

        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        if (contents.size() != firstReadSize || file.error() != QFileDevice::NoError)
            return {FileHashStatus::Failed, {}, QStringLiteral("Could not read the first sample")};

        // Read the last 64 KiB of a large file.
        if (size > 2 * chunkSize)
        {
            // Move the file’s read position to 64 KiB before its end.
            if (!file.seek(size - chunkSize))
                return {FileHashStatus::Failed, {}, file.errorString()};

            const QByteArray tail = file.read(chunkSize);

            if (cancellationToken.isCancellationRequested())
                return {FileHashStatus::Cancelled, {}, {}};

            if (tail.size() != chunkSize || file.error() != QFileDevice::NoError)
                return {FileHashStatus::Failed, {}, QStringLiteral("Could not read the last sample")};

            contents.append(tail);
        }

        // Refresh cached metadata and reject files that disappeared or changed
        // while their contents were being sampled.
        info.refresh();

        if (!info.exists() || info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {FileHashStatus::Failed, {}, QStringLiteral("File changed while sampling")};

        return {FileHashStatus::Completed, QCryptographicHash::hash(contents, QCryptographicHash::Sha256), {}};
    }

} // namespace DiskLens
