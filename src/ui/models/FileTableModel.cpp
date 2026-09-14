#include "FileTableModel.h"

#include <utility>

#include <QLocale>

namespace DiskLens
{

    FileTableModel::FileTableModel(QObject *parent)
        : QAbstractTableModel(parent) {}

    int FileTableModel::rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
            return 0;

        return static_cast<int>(entries_.size());
    }

    int FileTableModel::columnCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
            return 0;

        return ColumnCount;
    }

    QVariant FileTableModel::data(const QModelIndex &index, int role) const
    {
        if (!index.isValid() || index.row() >= entries_.size())
        {
            return {};
        }

        const FileEntry &entry = entries_.at(index.row());

        if (role == SortRole)
        {
            switch (index.column())
            {
            case NameColumn:
                return entry.name;

            case PathColumn:
                return entry.absolutePath;

            case SizeColumn:
                return entry.sizeBytes;

            case ModifiedColumn:
                return entry.lastModified;

            case KindColumn:
                if (entry.isSymbolicLink)
                    return QStringLiteral("Symbolic link");

                return entry.isDirectory ? QStringLiteral("Folder") : QStringLiteral("File");

            default:
                return {};
            }
        }

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        switch (index.column())
        {
        case NameColumn:
            return entry.name;

        case PathColumn:
            return entry.absolutePath;

        case SizeColumn:
            if (entry.isDirectory)
                return QStringLiteral("—");

            return QLocale().formattedDataSize(entry.sizeBytes);

        case ModifiedColumn:
            return QLocale().toString(entry.lastModified, QLocale::ShortFormat);

        case KindColumn:
            if (entry.isSymbolicLink)
                return QStringLiteral("Symbolic link");

            return entry.isDirectory ? QStringLiteral("Folder") : QStringLiteral("File");

        default:
            return {};
        }
    }

    QVariant FileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QAbstractTableModel::headerData(section, orientation, role);
        }

        switch (section)
        {
        case NameColumn:
            return QStringLiteral("Name");
        case PathColumn:
            return QStringLiteral("Path");
        case SizeColumn:
            return QStringLiteral("Size");
        case ModifiedColumn:
            return QStringLiteral("Modified");
        case KindColumn:
            return QStringLiteral("Kind");
        default:
            return {};
        }
    }

    void FileTableModel::setEntries(QVector<FileEntry> entries)
    {
        beginResetModel();             // Tell the table: data will change
        entries_ = std::move(entries); // Store the new files
        endResetModel();               // Tell the table: redraw now
    }

    const FileEntry *FileTableModel::entryAt(int row) const
    {
        if (row < 0 || row >= entries_.size())
        {
            return nullptr;
        }

        return &entries_.at(row);
    }

} // namespace DiskLens
