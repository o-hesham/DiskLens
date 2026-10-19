#pragma once

#include "CancellationToken.h"
#include "../analysis/DuplicateProgress.h"
#include "../analysis/DuplicateResult.h"
#include "../filesystem/FileEntry.h"

#include <QObject>
#include <QVector>
#include <memory>

namespace DiskLens
{

    class DuplicateJob final : public QObject
    {
        Q_OBJECT

    public:
        explicit DuplicateJob(QVector<FileEntry> entries, std::shared_ptr<CancellationToken> cancellationToken);

    public slots:
        void run();

    signals:
        void progressChanged(DiskLens::DuplicateProgress progress);
        void finished(DiskLens::DuplicateResult result);

    private:
        QVector<FileEntry> entries_;
        std::shared_ptr<CancellationToken> cancellationToken_;
    };

} // namespace DiskLens
