#include "DuplicateFinder.h"
#include "FileHasher.h"
#include "FileComparer.h"
#include "../filesystem/FileIdentity.h"

#include <QMap>

namespace DiskLens
{

    QVector<DuplicateCandidateGroup> DuplicateFinder::findSizeCandidates(const QVector<FileEntry> &entries)
    {
        QMap<qint64, QVector<FileEntry>> filesBySize;

        for (const auto &entry : entries)
        {
            if (entry.isDirectory || entry.isSymbolicLink || entry.sizeBytes <= 0)
                continue;

            filesBySize[entry.sizeBytes].append(entry);
        }

        QVector<DuplicateCandidateGroup> groups;

        for (auto it = filesBySize.cbegin(); it != filesBySize.cend(); ++it)
        {
            if (it.value().size() < 2)
                continue;

            groups.append({it.key(), it.value()});
        }

        return groups;
    }

    DuplicateResult DuplicateFinder::findDuplicates(const QVector<FileEntry> &entries, const CancellationToken &cancellationToken, const ProgressCallback &progressCallback)
    {
        DuplicateResult result;

        if (cancellationToken.isCancellationRequested())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        DuplicateProgress progress;
        progress.totalOperations = entries.size();

        const auto reportProgress = [&]()
        {
            if (progressCallback)
                progressCallback(progress);

            return !cancellationToken.isCancellationRequested();
        };

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        const auto sizeCandidates = findSizeCandidates(entries);

        progress.completedOperations = entries.size();

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        QVector<DuplicateCandidateGroup> sampleCandidates;

        progress = {DuplicatePhase::SampleHashing, {}, 0, 0};

        for (const auto &group : sizeCandidates)
            progress.totalOperations += group.files.size();

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        for (const auto &sizeGroup : sizeCandidates)
        {
            // Maps each sample hash to the files sharing it within this size group.
            QMap<QByteArray, QVector<FileEntry>> filesBySample;

            for (const auto &entry : sizeGroup.files)
            {
                progress.currentPath = entry.absolutePath;

                if (!reportProgress())
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                const auto hash = FileHasher::hashSample(entry, cancellationToken);
                if (hash.status == FileHashStatus::Cancelled)
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                ++progress.completedOperations;

                if (!reportProgress())
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                if (hash.status == FileHashStatus::Failed)
                {
                    result.issues.append({entry.absolutePath, hash.issueKind, hash.errorMessage});

                    continue;
                }

                // Adds the file to its hash group, creating the group if needed.
                filesBySample[hash.digest].append(entry);
            }

            // Go through the hash groups and keeps only those containing at least two files.
            for (auto it = filesBySample.cbegin(); it != filesBySample.cend(); ++it)
            {
                if (it.value().size() >= 2)
                    sampleCandidates.append({sizeGroup.sizeBytes, it.value()});
            }
        }

        QVector<DuplicateGroup> fullCandidates;

        progress = {DuplicatePhase::FullHashing, {}, 0, 0};

        for (const auto &group : sampleCandidates)
            progress.totalOperations += group.files.size();

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        for (const auto &sampleGroup : sampleCandidates)
        {
            QMap<QByteArray, QVector<FileEntry>> filesByFullHash;

            for (const auto &entry : sampleGroup.files)
            {
                progress.currentPath = entry.absolutePath;

                if (!reportProgress())
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                const auto hash = FileHasher::hashFull(entry, cancellationToken);
                if (hash.status == FileHashStatus::Cancelled)
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                ++progress.completedOperations;

                if (!reportProgress())
                {
                    result.status = DuplicateStatus::Cancelled;
                    return result;
                }

                if (hash.status == FileHashStatus::Failed)
                {
                    result.issues.append({entry.absolutePath, hash.issueKind, hash.errorMessage});

                    continue;
                }

                filesByFullHash[hash.digest].append(entry);
            }

            for (auto it = filesByFullHash.cbegin(); it != filesByFullHash.cend(); ++it)
            {
                if (it.value().size() >= 2)
                    fullCandidates.append({sampleGroup.sizeBytes, it.key(), it.value(), 0});
            }
        }

        QVector<DuplicateGroup> verifiedCandidates;

        progress = {DuplicatePhase::ByteComparison, {}, 0, -1};

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        for (const auto &fullGroup : fullCandidates)
        {
            auto pending = fullGroup.files;
            while (pending.size() >= 2)
            {
                const auto reference = pending.first();
                DuplicateGroup verified{fullGroup.sizeBytes, fullGroup.digest, {reference}, 0};
                QVector<FileEntry> differentFiles;
                bool retry = false;

                for (qsizetype i = 1; i < pending.size(); ++i)
                {
                    progress.currentPath = pending.at(i).absolutePath;

                    if (!reportProgress())
                    {
                        result.status = DuplicateStatus::Cancelled;
                        return result;
                    }

                    const auto comparison = FileComparer::compare(reference, pending.at(i), cancellationToken);

                    if (comparison.status == FileComparisonStatus::Cancelled)
                    {
                        result.status = DuplicateStatus::Cancelled;
                        return result;
                    }

                    ++progress.completedOperations;

                    if (!reportProgress())
                    {
                        result.status = DuplicateStatus::Cancelled;
                        return result;
                    }

                    if (comparison.status == FileComparisonStatus::Failed)
                    {
                        result.issues.append({comparison.failedPath, comparison.issueKind, comparison.errorMessage});

                        pending.removeIf([&](const FileEntry &file)
                                         { return file.absolutePath == comparison.failedPath; });

                        retry = true;
                        break;
                    }

                    if (comparison.status == FileComparisonStatus::Equal)
                        verified.files.append(pending.at(i));
                    else
                        differentFiles.append(pending.at(i));
                }

                if (retry)
                    continue;

                if (verified.files.size() >= 2)
                    verifiedCandidates.append(verified);

                pending = differentFiles;
            }
        }

        QVector<DuplicateGroup> confirmedGroups;

        progress = {DuplicatePhase::HardLinkChecking, {}, 0, -1};

        if (!reportProgress())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        for (const auto &verified : verifiedCandidates)
        {
            DuplicateGroup distinct{verified.sizeBytes, verified.digest, {}, 0};

            for (const auto &entry : verified.files)
            {
                bool alias = false;
                bool identityFailed = false;

                for (const auto &existing : distinct.files)
                {
                    progress.currentPath = entry.absolutePath;

                    if (!reportProgress())
                    {
                        result.status = DuplicateStatus::Cancelled;
                        return result;
                    }

                    std::error_code error;
                    const bool sameFile = referToSameFile(entry.absolutePath, existing.absolutePath, error);

                    ++progress.completedOperations;

                    if (!reportProgress())
                    {
                        result.status = DuplicateStatus::Cancelled;
                        return result;
                    }

                    if (error)
                    {
                        result.issues.append(
                            {entry.absolutePath,
                             DuplicateIssueKind::IdentityCheckFailed,
                             QStringLiteral("Identity comparison with %1 failed: %2")
                                 .arg(existing.absolutePath)
                                 .arg(QString::fromStdString(error.message()))});
                        identityFailed = true;
                        break;
                    }

                    if (sameFile)
                    {
                        alias = true;
                        ++distinct.ignoredHardLinkAliases;
                        break;
                    }
                }

                if (!alias && !identityFailed)
                    distinct.files.append(entry);
            }

            result.ignoredHardLinkAliases += distinct.ignoredHardLinkAliases;

            if (distinct.files.size() >= 2)
                confirmedGroups.append(distinct);
        }

        if (cancellationToken.isCancellationRequested())
        {
            result.status = DuplicateStatus::Cancelled;
            return result;
        }

        result.groups = confirmedGroups;
        result.status = DuplicateStatus::Completed;
        return result;
    }

} // namespace DiskLens