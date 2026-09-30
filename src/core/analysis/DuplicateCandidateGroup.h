#pragma once

#include "../filesystem/FileEntry.h"

#include <QVector>

namespace DiskLens
{

    struct DuplicateCandidateGroup
    {
        qint64 sizeBytes = 0;
        QVector<FileEntry> files;
    };

} // namespace DiskLens
