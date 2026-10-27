#include "RecoveryService.h"

#include <QDir>
#include <QStorageInfo>
#include <QFileInfo>

#include <algorithm>
#include <filesystem>

namespace DiskLens
{

    namespace
    {

        // Returns true if the file still exists with the same size and modified time it had when scanned.
        bool matchesScan(const QString &path, qint64 sizeBytes, const QDateTime &lastModified)
        {
            const QFileInfo info(path);
            return info.isFile() && info.size() == sizeBytes && info.lastModified() == lastModified;
        }

    } // namespace

    RecoveryService::RecoveryService(QString recoveryRootOverride)
        : recoveryRootOverride_(std::move(recoveryRootOverride))
    {
    }

    // Builds the list of planned moves for the marked files. Does not touch the disk.
    OperationRecord RecoveryService::buildPlan(const QVector<DuplicateGroup> &groups, const QSet<QString> &markedPaths)
    {
        OperationRecord record;
        record.id = QUuid::createUuid();
        record.createdAt = QDateTime::currentDateTimeUtc();
        keptCopies_.clear();

        int index = 0;
        for (const auto &group : groups)
        {
            QVector<FileEntry> kept;
            for (const auto &file : group.files)
            {
                if (!markedPaths.contains(file.absolutePath))
                    kept.append(file);
            }

            for (const auto &file : group.files)
            {
                if (!markedPaths.contains(file.absolutePath))
                    continue;

                RecoveryItem item;
                item.originalPath = file.absolutePath;
                item.sizeBytes = file.sizeBytes;
                item.lastModified = file.lastModified;
                item.digest = group.digest;

                const QString folder = operationFolderFor(file.absolutePath, record.id);
                if (folder.isEmpty())
                {
                    item.status = RecoveryStatus::Failed;
                    item.message = QStringLiteral("Could not determine the file's drive.");
                }
                else
                {
                    const QString name = QStringLiteral("%1_%2").arg(++index, 4, 10, QLatin1Char('0')).arg(file.name);
                    item.recoveryPath = QDir(folder).filePath(name);
                }

                keptCopies_.insert(file.absolutePath, kept);
                record.items.append(item);
            }
        }

        return record;
    }

    void RecoveryService::execute(OperationRecord &record) const
    {
        for (auto &item : record.items)
        {
            if (item.status != RecoveryStatus::Pending)
                continue;

            if (!matchesScan(item.originalPath, item.sizeBytes, item.lastModified))
            {
                item.status = RecoveryStatus::Skipped;
                item.message = QStringLiteral("Changed since scan.");
                continue;
            }

            const auto kept = keptCopies_.value(item.originalPath);
            const bool hasKeptCopy = std::any_of(kept.cbegin(), kept.cend(), [](const FileEntry &copy)
                                                 { return matchesScan(copy.absolutePath, copy.sizeBytes, copy.lastModified); });

            if (!hasKeptCopy)
            {
                item.status = RecoveryStatus::Skipped;
                item.message = QStringLiteral("No unchanged copy left to keep.");
                continue;
            }

            const QFileInfo destination(item.recoveryPath);
            if (!QDir().mkpath(destination.absolutePath()))
            {
                item.status = RecoveryStatus::Failed;
                item.message = QStringLiteral("Could not create the recovery folder.");
                continue;
            }

            if (destination.exists())
            {
                item.status = RecoveryStatus::Failed;
                item.message = QStringLiteral("A file already exists at the recovery path.");
                continue;
            }

            std::error_code error;
            std::filesystem::rename(QFileInfo(item.originalPath).filesystemFilePath(), destination.filesystemFilePath(), error);
            if (error)
            {
                item.status = RecoveryStatus::Failed;
                item.message = QString::fromLocal8Bit(error.message());
                continue;
            }

            item.status = RecoveryStatus::Moved;
        }
    }

    // Returns <drive root or override>/.DiskLens Recovery/<operation id>, or empty if the drive is unknown.
    QString RecoveryService::operationFolderFor(const QString &filePath, const QUuid &operationId) const
    {
        const QString root = recoveryRootOverride_.isEmpty() ? QStorageInfo(filePath).rootPath() : recoveryRootOverride_;
        if (root.isEmpty())
            return {};

        return QDir(root).filePath(kRecoveryFolderName + QLatin1Char('/') + operationId.toString(QUuid::WithoutBraces));
    }

} // namespace DiskLens