#pragma once

#include <QString>
#include <QtGlobal>
#include <QMetaType>

namespace DiskLens
{

    enum class DuplicatePhase
    {
        SizeGrouping,
        SampleHashing,
        FullHashing,
        ByteComparison,
        HardLinkChecking
    };

    struct DuplicateProgress
    {
        DuplicatePhase phase = DuplicatePhase::SizeGrouping;
        QString currentPath;
        qint64 completedOperations = 0;
        qint64 totalOperations = -1; //-1 means unknown; 0 means no work.
    };

} // namespace DiskLens

Q_DECLARE_METATYPE(DiskLens::DuplicateProgress);
