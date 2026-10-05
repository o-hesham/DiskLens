#include "FileIdentity.h"

#include <QFileInfo>
#include <filesystem>

namespace DiskLens
{
    bool referToSameFile(const QString &firstPath, const QString &secondPath, std::error_code &error)
    {
        const auto first = QFileInfo(firstPath).filesystemFilePath();
        const auto second = QFileInfo(secondPath).filesystemFilePath();

        const auto bothExist = [&]()
        {
            if (std::filesystem::exists(first, error) && std::filesystem::exists(second, error))
                return true;

            if (!error)
                error = std::make_error_code(std::errc::no_such_file_or_directory);

            return false;
        };

        if (!bothExist())
            return false;

        const bool sameFile = std::filesystem::equivalent(first, second, error);

        if (error || !bothExist())
            return false;

        return sameFile;
    }
}