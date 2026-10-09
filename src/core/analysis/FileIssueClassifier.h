#pragma once

#include "DuplicateIssueKind.h"
#include "../filesystem/FileEntry.h"

#include <QFileInfo>
#include <filesystem>

namespace DiskLens
{
    inline DuplicateIssueKind classifyFileFailure(const FileEntry &entry)
    {
        const QFileInfo info(entry.absolutePath);
        std::error_code error;
        const bool exists =
            std::filesystem::exists(info.filesystemFilePath(), error);

        if (error)
            return DuplicateIssueKind::Unreadable;

        if (!exists)
            return DuplicateIssueKind::Missing;

        if (!info.exists() || !info.lastModified().isValid())
            return DuplicateIssueKind::Unreadable;

        if (!info.isFile() || info.isSymbolicLink() ||
            entry.sizeBytes <= 0)
            return DuplicateIssueKind::Other;

        if (info.size() != entry.sizeBytes ||
            info.lastModified() != entry.lastModified)
            return DuplicateIssueKind::Changed;

        return DuplicateIssueKind::Unreadable;
    }
}