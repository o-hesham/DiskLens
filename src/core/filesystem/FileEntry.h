#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace DiskLens
{

    struct FileEntry
    {
        QString absolutePath;
        QString name;
        qint64 sizeBytes = 0;
        QDateTime lastModified;

        bool isDirectory = false;
        bool isSymbolicLink = false;
    };

} // namespace DiskLens
