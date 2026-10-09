#include "FileComparer.h"
#include "FileIssueClassifier.h"

#include <QFile>
#include <QFileInfo>

namespace DiskLens
{

    FileComparisonResult FileComparer::compare(const FileEntry &first, const FileEntry &second, const CancellationToken &cancellationToken)
    {
        if (cancellationToken.isCancellationRequested())
            return {FileComparisonStatus::Cancelled, {}};

        const auto matchesScan = [](const FileEntry &entry)
        {
            const QFileInfo info(entry.absolutePath);
            return info.isFile() && !info.isSymbolicLink() && entry.sizeBytes > 0 && info.size() == entry.sizeBytes && info.lastModified() == entry.lastModified;
        };

        if (!matchesScan(first))
            return {
                FileComparisonStatus::Failed,
                QStringLiteral("File missing, changed, or ineligible"),
                first.absolutePath,
                classifyFileFailure(first)};

        if (!matchesScan(second))
            return {
                FileComparisonStatus::Failed,
                QStringLiteral("File missing, changed, or ineligible"),
                second.absolutePath,
                classifyFileFailure(second)};

        if (first.sizeBytes != second.sizeBytes)
            return {FileComparisonStatus::Different, {}};

        QFile firstFile{first.absolutePath};
        QFile secondFile{second.absolutePath};

        if (!firstFile.open(QIODevice::ReadOnly))
            return {FileComparisonStatus::Failed, firstFile.errorString(), first.absolutePath, classifyFileFailure(first)};

        if (!secondFile.open(QIODevice::ReadOnly))
            return {FileComparisonStatus::Failed, secondFile.errorString(), second.absolutePath, classifyFileFailure(second)};

        constexpr qint64 chunkSize = 64 * 1024;
        qint64 remaining = first.sizeBytes;
        bool equal = true;

        while (remaining > 0)
        {
            if (cancellationToken.isCancellationRequested())
                return {FileComparisonStatus::Cancelled, {}};

            const qint64 requested = qMin(chunkSize, remaining);
            const QByteArray firstChunk = firstFile.read(requested);
            const QByteArray secondChunk = secondFile.read(requested);

            if (firstChunk.size() != requested || firstFile.error() != QFileDevice::NoError)
                return {FileComparisonStatus::Failed, QStringLiteral("Could not read comparison chunk"), first.absolutePath, classifyFileFailure(first)};

            if (secondChunk.size() != requested || secondFile.error() != QFileDevice::NoError)
                return {FileComparisonStatus::Failed, QStringLiteral("Could not read comparison chunk"), second.absolutePath, classifyFileFailure(second)};

            if (firstChunk != secondChunk)
            {
                equal = false;
                break;
            }

            remaining -= requested;
        }

        if (cancellationToken.isCancellationRequested())
            return {FileComparisonStatus::Cancelled, {}};

        if (!matchesScan(first))
            return {FileComparisonStatus::Failed, QStringLiteral("Could not validate file after comparing"), first.absolutePath, classifyFileFailure(first)};

        if (!matchesScan(second))
            return {FileComparisonStatus::Failed, QStringLiteral("Could not validate file after comparing"), second.absolutePath, classifyFileFailure(second)};

        return {equal ? FileComparisonStatus::Equal : FileComparisonStatus::Different, {}};
    }

} // namespace DiskLens
