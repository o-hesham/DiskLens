#pragma once

#include "DuplicateCandidateGroup.h"
#include "DuplicateResult.h"
#include "../jobs/CancellationToken.h"
#include "DuplicateProgress.h"

#include <functional>

namespace DiskLens
{

    class DuplicateFinder final
    {
    public:
        [[nodiscard]] static QVector<DuplicateCandidateGroup> findSizeCandidates(const QVector<FileEntry> &entries);

        using ProgressCallback = std::function<void(const DuplicateProgress &)>;

        [[nodiscard]] static DuplicateResult findDuplicates(const QVector<FileEntry> &entries, const CancellationToken &cancellationToken, const ProgressCallback &progressCallback = {});
    };

} // namespace DiskLens
