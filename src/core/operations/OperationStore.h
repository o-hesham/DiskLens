#pragma once

#include "OperationRecord.h"

#include <optional>

namespace DiskLens
{

    // Saves and loads operation records as one JSON file per operation.
    class OperationStore final
    {
    public:
        // An empty directory uses AppLocalDataLocation/operations; tests pass a temporary folder.
        explicit OperationStore(QString directory = {});

        [[nodiscard]] bool save(const OperationRecord &record, QString *errorMessage = nullptr) const;
        [[nodiscard]] std::optional<OperationRecord> load(const QUuid &id) const;

    private:
        [[nodiscard]] QString filePathFor(const QUuid &id) const;

        QString directory_;
    };

} // namespace DiskLens
