#pragma once

#include <QString>
#include <system_error>

namespace DiskLens
{

    [[nodiscard]] bool referToSameFile(const QString &firstPath, const QString &secondPath, std::error_code &error);

} // namespace DiskLens
