#pragma once

#include "../filesystem/FileEntry.h"
#include "../filesystem/ScanResult.h"

#include <memory>
#include <vector>

namespace DiskLens
{

    struct StorageNode final
    {
        FileEntry entry;

        qint64 totalSizeBytes = 0;
        qsizetype fileCount = 0;

        StorageNode *parent = nullptr;
        std::vector<std::unique_ptr<StorageNode>> children;
    };

    class StorageTree final
    {
    public:
        void build(const ScanResult &result);

        [[nodiscard]] const StorageNode *root() const noexcept;

    private:
        std::unique_ptr<StorageNode> root_;
    };

} // namespace DiskLens
