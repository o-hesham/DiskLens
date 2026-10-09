#include "FileHasher.h"
#include "FileIssueClassifier.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <filesystem>

namespace DiskLens
{

    // Hash the first and last 64 KiB to quickly filter duplicate candidates.
    // Files up to 128 KiB are hashed completely once.
    FileHashResult FileHasher::hashSample(const FileEntry &entry, const CancellationToken &cancellationToken)
    {
        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        QFile file(entry.absolutePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            std::error_code error;
            const bool exists = std::filesystem::exists(
                QFileInfo(entry.absolutePath).filesystemFilePath(), error);

            const auto kind = !error && !exists ? DuplicateIssueKind::Missing : DuplicateIssueKind::Unreadable;

            return {FileHashStatus::Failed, {}, file.errorString(), kind};
        }

        QFileInfo info(file);

        // Reject files whose size or modification time changed since the scan.
        if (info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {FileHashStatus::Failed, {}, QStringLiteral("File changed since the scan"), DuplicateIssueKind::Changed};

        const qint64 size = file.size();
        constexpr qint64 chunkSize = 64 * 1024;

        if (size <= 0)
            return {FileHashStatus::Failed, {}, QStringLiteral("Expected a non-empty file")};

        const qint64 firstReadSize = size <= 2 * chunkSize ? size : chunkSize;

        QByteArray contents = file.read(firstReadSize);

        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        if (contents.size() != firstReadSize || file.error() != QFileDevice::NoError)
            return {FileHashStatus::Failed, {}, QStringLiteral("Could not read the first sample"), classifyFileFailure(entry)};

        // Read the last 64 KiB of a large file.
        if (size > 2 * chunkSize)
        {
            // Move the file’s read position to 64 KiB before its end.
            if (!file.seek(size - chunkSize))
                return {
                    FileHashStatus::Failed,
                    {},
                    file.errorString(),
                    classifyFileFailure(entry)};

            const QByteArray tail = file.read(chunkSize);

            if (cancellationToken.isCancellationRequested())
                return {FileHashStatus::Cancelled, {}, {}};

            if (tail.size() != chunkSize || file.error() != QFileDevice::NoError)
                return {FileHashStatus::Failed, {}, QStringLiteral("Could not read the last sample"), classifyFileFailure(entry)};

            contents.append(tail);
        }

        // Refresh cached metadata and reject files that disappeared or changed
        // while their contents were being sampled.
        info.refresh();

        if (!info.exists() || !info.lastModified().isValid())
            return {
                FileHashStatus::Failed,
                {},
                QStringLiteral("Could not validate file after reading"),
                classifyFileFailure(entry)};

        if (info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {
                FileHashStatus::Failed,
                {},
                QStringLiteral("File changed while reading"),
                DuplicateIssueKind::Changed};

        return {FileHashStatus::Completed, QCryptographicHash::hash(contents, QCryptographicHash::Sha256), {}};
    }

    // Hash every byte of the file in chunks to check full-content matches.
    FileHashResult FileHasher::hashFull(const FileEntry &entry, const CancellationToken &cancellationToken)
    {
        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        QFile file(entry.absolutePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            std::error_code error;
            const bool exists = std::filesystem::exists(
                QFileInfo(entry.absolutePath).filesystemFilePath(), error);

            const auto kind = !error && !exists
                                  ? DuplicateIssueKind::Missing
                                  : DuplicateIssueKind::Unreadable;

            return {FileHashStatus::Failed, {}, file.errorString(), kind};
        }

        QFileInfo info(file);

        if (info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {
                FileHashStatus::Failed,
                {},
                QStringLiteral("File changed since the scan"),
                DuplicateIssueKind::Changed};

        if (entry.sizeBytes <= 0)
            return {FileHashStatus::Failed, {}, QStringLiteral("Expected a non-empty file")};

        QCryptographicHash hash(QCryptographicHash::Sha256);
        constexpr qint64 chunkSize = 64 * 1024;
        qint64 remaining = entry.sizeBytes;

        while (remaining > 0)
        {
            if (cancellationToken.isCancellationRequested())
                return {FileHashStatus::Cancelled, {}, {}};

            const qint64 requested = qMin(chunkSize, remaining);
            const QByteArray chunk = file.read(requested);

            if (chunk.size() != requested || file.error() != QFileDevice::NoError)
                return {FileHashStatus::Failed, {}, QStringLiteral("Could not read the complete file"), classifyFileFailure(entry)};

            hash.addData(chunk);
            remaining -= chunk.size();
        }

        if (cancellationToken.isCancellationRequested())
            return {FileHashStatus::Cancelled, {}, {}};

        info.refresh();

        if (!info.exists() || !info.lastModified().isValid())
            return {
                FileHashStatus::Failed,
                {},
                QStringLiteral("Could not validate file after reading"),
                classifyFileFailure(entry)};

        if (info.size() != entry.sizeBytes || info.lastModified() != entry.lastModified)
            return {
                FileHashStatus::Failed,
                {},
                QStringLiteral("File changed while reading"),
                DuplicateIssueKind::Changed};

        return {FileHashStatus::Completed, hash.result(), {}};
    }

} // namespace DiskLens
