#pragma once

#include "CancellationToken.h"
#include "../filesystem/ScanResult.h"

#include <QObject>
#include <QString>

#include <memory>

namespace DiskLens
{

    class ScanJob final : public QObject
    {
        Q_OBJECT

    public:
        explicit ScanJob(QString rootPath, std::shared_ptr<CancellationToken> cancellationToken);

    public slots:
        void run();

    signals:
        void finished(DiskLens::ScanResult result);

    private:
        QString rootPath_;
        std::shared_ptr<CancellationToken> cancellationToken_;
    };

} // namespace DiskLens
