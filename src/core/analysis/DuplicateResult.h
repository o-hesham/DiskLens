#pragma once

#include "DuplicateGroup.h"

#include <QString>
#include <QVector>

namespace DiskLens
{
    // Describes how the entire duplicate-analysis operation ended.
    enum class DuplicateStatus
    {
        Completed,
        Cancelled,
        Failed
    };

    // Categorizes the problem that caused a file to be skipped.
    enum class DuplicateIssueKind
    {
        Missing,
        Unreadable,
        Changed,
        IdentityCheckFailed, // Its physical identity could not be established.
        Other
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
