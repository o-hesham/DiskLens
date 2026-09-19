#pragma once

#include <QAbstractItemModel>

namespace DiskLens
{
    struct StorageNode;

    class StorageTreeModel final : public QAbstractItemModel
    {
    public:
        enum Column
        {
            NameColumn,
            SizeColumn,
            FileCountColumn,
            ColumnCount
        };

        explicit StorageTreeModel(QObject *parent = nullptr);

        QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
        QModelIndex parent(const QModelIndex &index) const override;
        int rowCount(const QModelIndex &parent = {}) const override;
        int columnCount(const QModelIndex &parent = {}) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

        void setRoot(const StorageNode *root);
        [[nodiscard]] const StorageNode *nodeAt(const QModelIndex &index) const;

    private:
        const StorageNode *root_ = nullptr;
    };

} // namespace DiskLens
