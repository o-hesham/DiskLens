#pragma once

#include "DuplicateGroup.h"
#include "DuplicateIssueKind.h"

#include <QString>
#include <QVector>
#include <QMetaType>

namespace DiskLens
{
    // Describes how the entire duplicate-analysis operation ended.
    enum class DuplicateStatus
    {
        Completed,
        Cancelled,
        Failed
    };

    // Records one file-specific problem without failing the whole analysis.
    struct DuplicateIssue
    {
        QString absolutePath;
        DuplicateIssueKind kind = DuplicateIssueKind::Other;
        QString message;
    };

    // Contains the outcome, confirmed groups, and issues from one analysis.
    struct DuplicateResult
    {
        DuplicateStatus status = DuplicateStatus::Failed;
        QVector<DuplicateGroup> groups;
        QVector<DuplicateIssue> issues;
        qsizetype ignoredHardLinkAliases = 0;
        QString errorMessage;
    };

} // namespace DiskLens

Q_DECLARE_METATYPE(DiskLens::DuplicateResult);
