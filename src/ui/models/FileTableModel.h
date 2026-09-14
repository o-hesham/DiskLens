#pragma once

#include "../../core/filesystem/FileEntry.h"

#include <QAbstractTableModel>
#include <QVector>

namespace DiskLens
{

    class FileTableModel final : public QAbstractTableModel
    {
    public:
        enum Column
        {
            NameColumn,
            PathColumn,
            SizeColumn,
            ModifiedColumn,
            KindColumn,
            ColumnCount
        };

        enum Role
        {
            SortRole = Qt::UserRole + 1
        };

        explicit FileTableModel(QObject *parent = nullptr);

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;

        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

        void setEntries(QVector<FileEntry> entries);

        [[nodiscard]] const FileEntry *entryAt(int row) const;

    private:
        QVector<FileEntry> entries_;
    };

} // namespace DiskLens
