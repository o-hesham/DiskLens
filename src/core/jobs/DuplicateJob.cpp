#include "DuplicateJob.h"
#include "../analysis/DuplicateFinder.h"

#include <QElapsedTimer>

#include <exception>
#include <utility>

namespace DiskLens
{

    DuplicateJob::DuplicateJob(QVector<FileEntry> entries, std::shared_ptr<CancellationToken> cancellationToken)
        : entries_(std::move(entries)), cancellationToken_(std::move(cancellationToken))
    {
        if (!cancellationToken_)
            cancellationToken_ = std::make_shared<CancellationToken>();
    }

    void DuplicateJob::run()
    {
        DuplicateResult result;

        QElapsedTimer progressTimer;
        DuplicatePhase lastPhase = DuplicatePhase::SizeGrouping;

        try
        {
            result = DuplicateFinder::findDuplicates(
                entries_, *cancellationToken_,
                [this, &progressTimer, &lastPhase](const DuplicateProgress &progress)
                {
                    const bool phaseFinished = progress.totalOperations >= 0 && progress.completedOperations == progress.totalOperations;

                    if (!progressTimer.isValid() || progress.phase != lastPhase || phaseFinished || progressTimer.elapsed() >= 100)
                    {
                        emit progressChanged(progress);
                        lastPhase = progress.phase;
                        progressTimer.start();
                    }
                });
        }
        catch (const std::exception &error)
        {
            result.errorMessage = QString::fromLocal8Bit(error.what());
        }
        catch (...)
        {
            result.errorMessage = QStringLiteral("Unexpected duplicate-analysis error");
        }

        emit finished(std::move(result));
    }

} // namespace DiskLens
