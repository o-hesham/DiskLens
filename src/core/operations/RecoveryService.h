#pragma once

#include "OperationRecord.h"
#include "../analysis/DuplicateGroup.h"

#include <QHash>
#include <QSet>

namespace DiskLens
{

    // Plans and performs rename-only moves of marked duplicates into a recovery folder.
    class RecoveryService final
    {
    public:
        // An empty override uses each file's drive root; tests pass a temporary folder.
        explicit RecoveryService(QString recoveryRootOverride = {});

        [[nodiscard]] OperationRecord buildPlan(const QVector<DuplicateGroup> &groups, const QSet<QString> &markedPaths);
        // Moves each pending item by rename only and records its results; never copies or overwrites.
        void execute(OperationRecord &record) const;

    private:
        [[nodiscard]] QString operationFolderFor(const QString &filepath, const QUuid &operationId) const;

        QString recoveryRootOverride_;
        QHash<QString, QVector<FileEntry>> keptCopies_;
    };

} // namespace DiskLens
