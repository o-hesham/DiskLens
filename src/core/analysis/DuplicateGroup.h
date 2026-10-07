#pragma once

#include "../filesystem/FileEntry.h"

#include <QByteArray>
#include <QVector>

namespace DiskLens
{

    struct DuplicateGroup
    {
        qint64 sizeBytes = 0;
        QByteArray digest;
        QVector<FileEntry> files;
        qsizetype ignoredHardLinkAliases = 0;
    };

} // namespace DiskLens
