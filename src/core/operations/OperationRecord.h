#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QtGlobal>

namespace DiskLens
{

    // Name of the per-drive folder that holds moved files; the scanner skips it.
    inline constexpr QLatin1StringView kRecoveryFolderName{".DiskLens Recovery"};

    enum class RecoveryStatus
    {
        Pending,
        Moved,
        Skipped,
        Failed
    };

    struct RecoveryItem
    {
        QString originalPath;
        QString recoveryPath;
        qint64 sizeBytes = 0;
        QDateTime lastModified;
        QByteArray digest;

        RecoveryStatus status = RecoveryStatus::Pending;
        QString message;
    };

    struct OperationRecord
    {
        QUuid id;
        QDateTime createdAt;
        QVector<RecoveryItem> items;
    };

} // namespace DiskLens
