#include "DuplicateFinder.h"

#include <QMap>

namespace DiskLens
{

    QVector<DuplicateCandidateGroup> DuplicateFinder::findSizeCandidates(const QVector<FileEntry> &entries)
    {
        QMap<qint64, QVector<FileEntry>> filesBySize;

        for (const auto &entry : entries)
        {
            if (entry.isDirectory || entry.isSymbolicLink || entry.sizeBytes <= 0)
                continue;

            filesBySize[entry.sizeBytes].append(entry);
        }

        QVector<DuplicateCandidateGroup> groups;

        for (auto it = filesBySize.cbegin(); it != filesBySize.cend(); ++it)
        {
            if (it.value().size() < 2)
                continue;

            groups.append({it.key(), it.value()});
        }

        return groups;
    }

} // namespace DiskLens