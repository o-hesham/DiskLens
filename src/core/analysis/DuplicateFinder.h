#pragma once

#include "DuplicateCandidateGroup.h"
#include "DuplicateResult.h"
#include "../jobs/CancellationToken.h"

namespace DiskLens
{

    class DuplicateFinder final
    {
    public:
        [[nodiscard]] static QVector<DuplicateCandidateGroup> findSizeCandidates(const QVector<FileEntry> &entries);
        [[nodiscard]] static DuplicateResult findDuplicates(const QVector<FileEntry> &entries, const CancellationToken &cancellationToken);
    };

} // namespace DiskLens
