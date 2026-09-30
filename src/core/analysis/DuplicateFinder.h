#pragma once

#include "DuplicateCandidateGroup.h"

namespace DiskLens
{

    class DuplicateFinder final
    {
    public:
        [[nodiscard]] static QVector<DuplicateCandidateGroup> findSizeCandidates(const QVector<FileEntry> &entries);
    };

} // namespace DiskLens
