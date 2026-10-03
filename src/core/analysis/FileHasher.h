#pragma once

#include "FileHashResult.h"
#include "../filesystem/FileEntry.h"
#include "../jobs/CancellationToken.h"

namespace DiskLens
{

    class FileHasher final
    {
    public:
        [[nodiscard]] static FileHashResult hashSample(const FileEntry &entry, const CancellationToken &cancellationToken);
    };

} // namespace DiskLens
